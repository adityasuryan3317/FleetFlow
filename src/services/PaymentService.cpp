#include "fleetflow/services/PaymentService.hpp"

#include "fleetflow/core/Exceptions.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace fleetflow::services {

namespace {

bool isValidPaymentType(core::PaymentType type) noexcept {
    return type == core::PaymentType::Payment || type == core::PaymentType::Refund;
}

bool isValidPaymentMethod(core::PaymentMethod method) noexcept {
    return method == core::PaymentMethod::Cash ||
           method == core::PaymentMethod::Card ||
           method == core::PaymentMethod::UPI;
}

double signedAmount(const models::PaymentRecord& transaction) noexcept {
    return transaction.getPaymentType() == core::PaymentType::Refund
        ? -transaction.getAmount()
        : transaction.getAmount();
}

} // namespace

PaymentService::PaymentService(
    storage::Repository<models::PaymentRecord>& pRepo,
    storage::Repository<models::RentalRecord>& rRepo,
    storage::Repository<models::User>& uRepo) noexcept
    : paymentRepo(pRepo), rentalRepo(rRepo), userRepo(uRepo) {}

models::RentalRecord& PaymentService::requireRental(int rentalId) const {
    models::RentalRecord* rental = rentalRepo.getById(rentalId);
    if (rental == nullptr) {
        throw core::EntityNotFoundException(
            "Rental #" + std::to_string(rentalId) + " not found.");
    }
    return *rental;
}

models::User& PaymentService::requireCustomer(int customerId) const {
    models::User* customer = userRepo.getById(customerId);
    if (customer == nullptr) {
        throw core::EntityNotFoundException(
            "Customer/user #" + std::to_string(customerId) + " not found.");
    }
    return *customer;
}

models::PaymentRecord* PaymentService::recordTransaction(
    std::unique_ptr<models::PaymentRecord> transaction) {
    if (!transaction) {
        throw core::ValidationException("Cannot record a null payment transaction.");
    }

    const int paymentId = transaction->getPaymentId();
    const int rentalId = transaction->getRentalId();
    const int customerId = transaction->getCustomerId();

    if (paymentRepo.exists(paymentId)) {
        throw core::DuplicateEntityException(
            "Payment transaction with ID #" + std::to_string(paymentId) +
            " already exists.");
    }

    models::RentalRecord& rental = requireRental(rentalId);
    (void)requireCustomer(customerId);

    if (rental.getCustomerId() != customerId) {
        throw core::ValidationException(
            "Payment transaction customer #" + std::to_string(customerId) +
            " does not match rental #" + std::to_string(rentalId) +
            " customer #" + std::to_string(rental.getCustomerId()) + ".");
    }

    const double amount = transaction->getAmount();
    if (!std::isfinite(amount) || amount <= 0.0) {
        throw core::ValidationException(
            "Payment transaction amount must be finite and positive.");
    }
    if (!isValidPaymentType(transaction->getPaymentType())) {
        throw core::ValidationException("Payment transaction type is invalid.");
    }
    if (!isValidPaymentMethod(transaction->getPaymentMethod())) {
        throw core::ValidationException("Payment method is invalid.");
    }

    paymentRepo.add(std::move(transaction));
    return paymentRepo.getById(paymentId);
}

models::PaymentRecord* PaymentService::getTransaction(int paymentId) const noexcept {
    return paymentRepo.getById(paymentId);
}

std::vector<models::PaymentRecord*> PaymentService::listTransactions() const {
    return paymentRepo.getAll();
}

std::vector<models::PaymentRecord*> PaymentService::listTransactionsForRental(
    int rentalId) const {
    (void)requireRental(rentalId);

    std::vector<models::PaymentRecord*> result;
    for (models::PaymentRecord* transaction : paymentRepo.getAll()) {
        if (transaction->getRentalId() == rentalId) {
            result.push_back(transaction);
        }
    }
    return result;
}

std::vector<models::PaymentRecord*> PaymentService::listTransactionsForCustomer(
    int customerId) const {
    (void)requireCustomer(customerId);

    std::vector<models::PaymentRecord*> result;
    for (models::PaymentRecord* transaction : paymentRepo.getAll()) {
        if (transaction->getCustomerId() == customerId) {
            result.push_back(transaction);
        }
    }
    return result;
}

double PaymentService::getNetAmountForRental(int rentalId) const {
    (void)requireRental(rentalId);

    double total = 0.0;
    for (const models::PaymentRecord* transaction : paymentRepo.getAll()) {
        if (transaction->getRentalId() == rentalId) {
            total += signedAmount(*transaction);
        }
    }
    return total;
}

double PaymentService::getNetAmountForCustomer(int customerId) const {
    (void)requireCustomer(customerId);

    double total = 0.0;
    for (const models::PaymentRecord* transaction : paymentRepo.getAll()) {
        if (transaction->getCustomerId() == customerId) {
            total += signedAmount(*transaction);
        }
    }
    return total;
}

} // namespace fleetflow::services
