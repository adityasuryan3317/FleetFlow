#include "fleetflow/services/PaymentService.hpp"

#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Customer.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>

namespace {

using namespace fleetflow;

struct Repositories {
    storage::Repository<models::PaymentRecord> payments;
    storage::Repository<models::RentalRecord> rentals;
    storage::Repository<models::User> users;
    services::PaymentService service{payments, rentals, users};
};

std::unique_ptr<models::User> makeCustomer(int id) {
    return std::make_unique<models::Customer>(
        id, "customer" + std::to_string(id), "Test Customer",
        "customer" + std::to_string(id) + "@example.com", "DL-123", "9999999999");
}

std::unique_ptr<models::RentalRecord> makeRental(int rentalId, int customerId) {
    return std::make_unique<models::RentalRecord>(
        rentalId, customerId, 3,
        core::DateRange(core::Date(2026, 1, 10), core::Date(2026, 1, 12)), 100.0);
}

std::unique_ptr<models::PaymentRecord> makeTransaction(
    int paymentId, int rentalId = 1, int customerId = 7,
    double amount = 50.0, core::PaymentType type = core::PaymentType::Payment,
    core::PaymentMethod method = core::PaymentMethod::Cash) {
    return std::make_unique<models::PaymentRecord>(
        paymentId, rentalId, customerId, amount, core::Date(2026, 1, 10),
        type, method, type == core::PaymentType::Refund ? "Rental refund" : "Rental payment");
}

void addCustomerAndRental(Repositories& repos, int customerId = 7, int rentalId = 1) {
    repos.users.add(makeCustomer(customerId));
    repos.rentals.add(makeRental(rentalId, customerId));
}

} // namespace

TEST(PaymentServiceRecordTest, RecordsPaymentForExistingRentalAndCustomer) {
    Repositories repos;
    addCustomerAndRental(repos);

    auto* stored = repos.service.recordTransaction(makeTransaction(10));
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->getPaymentId(), 10);
    EXPECT_EQ(repos.payments.size(), 1U);
}

TEST(PaymentServiceRecordTest, RejectsNullTransaction) {
    Repositories repos;
    std::unique_ptr<models::PaymentRecord> transaction;
    EXPECT_THROW(repos.service.recordTransaction(std::move(transaction)), core::ValidationException);
}

TEST(PaymentServiceRecordTest, RejectsMissingRental) {
    Repositories repos;
    repos.users.add(makeCustomer(7));
    EXPECT_THROW(repos.service.recordTransaction(makeTransaction(1)), core::EntityNotFoundException);
}

TEST(PaymentServiceRecordTest, RejectsMissingCustomer) {
    Repositories repos;
    repos.rentals.add(makeRental(1, 7));
    EXPECT_THROW(repos.service.recordTransaction(makeTransaction(1)), core::EntityNotFoundException);
}

TEST(PaymentServiceRecordTest, RejectsCustomerThatDoesNotMatchRental) {
    Repositories repos;
    addCustomerAndRental(repos, 7, 1);
    repos.users.add(makeCustomer(8));
    EXPECT_THROW(repos.service.recordTransaction(makeTransaction(1, 1, 8)), core::ValidationException);
    EXPECT_TRUE(repos.payments.empty());
}

TEST(PaymentServiceRecordTest, RejectsDuplicatePaymentId) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.service.recordTransaction(makeTransaction(1));
    EXPECT_THROW(repos.service.recordTransaction(makeTransaction(1)), core::DuplicateEntityException);
}

TEST(PaymentServiceRecordTest, RejectsNaNAmount) {
    Repositories repos;
    addCustomerAndRental(repos);
    EXPECT_THROW(repos.service.recordTransaction(
        makeTransaction(1, 1, 7, std::numeric_limits<double>::quiet_NaN())), core::ValidationException);
}

TEST(PaymentServiceRecordTest, RejectsInfiniteAmount) {
    Repositories repos;
    addCustomerAndRental(repos);
    EXPECT_THROW(repos.service.recordTransaction(
        makeTransaction(1, 1, 7, std::numeric_limits<double>::infinity())), core::ValidationException);
}

TEST(PaymentServiceRecordTest, RejectsInvalidPaymentType) {
    Repositories repos;
    addCustomerAndRental(repos);
    EXPECT_THROW(repos.service.recordTransaction(
        makeTransaction(1, 1, 7, 50.0, static_cast<core::PaymentType>(99))), core::ValidationException);
}

TEST(PaymentServiceRecordTest, RejectsInvalidPaymentMethod) {
    Repositories repos;
    addCustomerAndRental(repos);
    EXPECT_THROW(repos.service.recordTransaction(
        makeTransaction(1, 1, 7, 50.0, core::PaymentType::Payment,
                        static_cast<core::PaymentMethod>(99))), core::ValidationException);
}

TEST(PaymentServiceQueryTest, GetsTransactionAndReturnsNullForMissingId) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.service.recordTransaction(makeTransaction(1));
    EXPECT_NE(repos.service.getTransaction(1), nullptr);
    EXPECT_EQ(repos.service.getTransaction(404), nullptr);
}

TEST(PaymentServiceQueryTest, ListsAllTransactions) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.service.recordTransaction(makeTransaction(1));
    repos.service.recordTransaction(makeTransaction(2, 1, 7, 25.0));
    EXPECT_EQ(repos.service.listTransactions().size(), 2U);
}

TEST(PaymentServiceQueryTest, ListsTransactionsForRental) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.rentals.add(makeRental(2, 7));
    repos.service.recordTransaction(makeTransaction(1, 1, 7));
    repos.service.recordTransaction(makeTransaction(2, 2, 7));
    EXPECT_EQ(repos.service.listTransactionsForRental(1).size(), 1U);
}

TEST(PaymentServiceQueryTest, ListsTransactionsForCustomer) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.users.add(makeCustomer(8));
    repos.rentals.add(makeRental(2, 8));
    repos.service.recordTransaction(makeTransaction(1, 1, 7));
    repos.service.recordTransaction(makeTransaction(2, 2, 8));
    EXPECT_EQ(repos.service.listTransactionsForCustomer(7).size(), 1U);
}

TEST(PaymentServiceQueryTest, MissingRentalQueryThrows) {
    Repositories repos;
    EXPECT_THROW(repos.service.listTransactionsForRental(404), core::EntityNotFoundException);
    EXPECT_THROW(repos.service.getNetAmountForRental(404), core::EntityNotFoundException);
}

TEST(PaymentServiceQueryTest, MissingCustomerQueryThrows) {
    Repositories repos;
    EXPECT_THROW(repos.service.listTransactionsForCustomer(404), core::EntityNotFoundException);
    EXPECT_THROW(repos.service.getNetAmountForCustomer(404), core::EntityNotFoundException);
}

TEST(PaymentServiceTotalsTest, CalculatesNetAmountForRental) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.service.recordTransaction(makeTransaction(1, 1, 7, 100.0, core::PaymentType::Payment));
    repos.service.recordTransaction(makeTransaction(2, 1, 7, 25.0, core::PaymentType::Refund));
    EXPECT_DOUBLE_EQ(repos.service.getNetAmountForRental(1), 75.0);
}

TEST(PaymentServiceTotalsTest, CalculatesNetAmountForCustomerAcrossRentals) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.rentals.add(makeRental(2, 7));
    repos.service.recordTransaction(makeTransaction(1, 1, 7, 100.0, core::PaymentType::Payment));
    repos.service.recordTransaction(makeTransaction(2, 2, 7, 20.0, core::PaymentType::Refund));
    EXPECT_DOUBLE_EQ(repos.service.getNetAmountForCustomer(7), 80.0);
}

TEST(PaymentServiceTotalsTest, RefundOnlyLedgerProducesNegativeNetAmount) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.service.recordTransaction(makeTransaction(1, 1, 7, 20.0, core::PaymentType::Refund));
    EXPECT_DOUBLE_EQ(repos.service.getNetAmountForRental(1), -20.0);
}

TEST(PaymentServiceRecordTest, SupportsAllPaymentMethods) {
    Repositories repos;
    addCustomerAndRental(repos);
    repos.service.recordTransaction(makeTransaction(1, 1, 7, 10.0, core::PaymentType::Payment,
                                                     core::PaymentMethod::Cash));
    repos.service.recordTransaction(makeTransaction(2, 1, 7, 10.0, core::PaymentType::Payment,
                                                     core::PaymentMethod::Card));
    repos.service.recordTransaction(makeTransaction(3, 1, 7, 10.0, core::PaymentType::Payment,
                                                     core::PaymentMethod::UPI));
    EXPECT_EQ(repos.service.listTransactions().size(), 3U);
}
