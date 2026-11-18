#include "fleetflow/services/FleetService.hpp"

#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/services/Exceptions.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace {

using namespace fleetflow;

struct Repositories {
    storage::Repository<models::Vehicle> vehicles;
    storage::Repository<models::User> users;
    storage::Repository<models::RentalRecord> rentals;
    storage::Repository<models::PaymentRecord> payments;
    storage::Repository<models::MaintenanceRecord> maintenance;
    services::FleetService service{
        vehicles, users, rentals, payments, maintenance
    };
};

std::unique_ptr<models::Vehicle> makeCar(int id) {
    return std::make_unique<models::Car>(
        id, "Toyota", "Corolla", "CAR-" + std::to_string(id),
        50.0, 5, "Petrol", true);
}

std::unique_ptr<models::User> makeCustomer(int id) {
    return std::make_unique<models::Customer>(
        id, "customer" + std::to_string(id), "Test Customer",
        "customer" + std::to_string(id) + "@example.com", "DL-123", "9999999999");
}

fleetflow::core::DateRange sampleRange() {
    return fleetflow::core::DateRange(
        fleetflow::core::Date(2026, 1, 10), fleetflow::core::Date(2026, 1, 12));
}

} // namespace

TEST(FleetServiceVehicleTest, AddsGetsAndListsVehicles) {
    Repositories repos;
    models::Vehicle* added = repos.service.addVehicle(makeCar(1));

    ASSERT_NE(added, nullptr);
    EXPECT_EQ(added->getVehicleId(), 1);
    EXPECT_EQ(repos.service.getVehicle(1), added);
    EXPECT_EQ(repos.service.getVehicle(99), nullptr);

    const auto vehicles = repos.service.listVehicles();
    ASSERT_EQ(vehicles.size(), 1U);
    EXPECT_EQ(vehicles.front(), added);
}

TEST(FleetServiceVehicleTest, RejectsNullVehicle) {
    Repositories repos;
    std::unique_ptr<models::Vehicle> vehicle;
    EXPECT_THROW(repos.service.addVehicle(std::move(vehicle)), core::ValidationException);
}

TEST(FleetServiceVehicleTest, PropagatesDuplicateVehicleIdException) {
    Repositories repos;
    repos.service.addVehicle(makeCar(1));
    EXPECT_THROW(repos.service.addVehicle(makeCar(1)), core::DuplicateEntityException);
    EXPECT_EQ(repos.vehicles.size(), 1U);
}

TEST(FleetServiceVehicleTest, RemovesUnreferencedVehicle) {
    Repositories repos;
    repos.service.addVehicle(makeCar(1));
    EXPECT_NO_THROW(repos.service.removeVehicle(1));
    EXPECT_EQ(repos.service.getVehicle(1), nullptr);
}

TEST(FleetServiceVehicleTest, ThrowsForMissingVehicle) {
    Repositories repos;
    EXPECT_THROW(repos.service.removeVehicle(404), core::EntityNotFoundException);
}

TEST(FleetServiceVehicleTest, PreventsRemovalWhenRentalReferencesVehicle) {
    Repositories repos;
    repos.service.addVehicle(makeCar(1));
    repos.rentals.add(std::make_unique<models::RentalRecord>(
        10, 20, 1, sampleRange(), 100.0));

    EXPECT_THROW(repos.service.removeVehicle(1), services::BusinessException);
    EXPECT_NE(repos.service.getVehicle(1), nullptr);
}

TEST(FleetServiceVehicleTest, PreventsRemovalWhenMaintenanceReferencesVehicle) {
    Repositories repos;
    repos.service.addVehicle(makeCar(1));
    repos.maintenance.add(std::make_unique<models::MaintenanceRecord>(
        30, 1, "Brake inspection", core::Date(2026, 1, 5), 25.0));

    EXPECT_THROW(repos.service.removeVehicle(1), services::BusinessException);
    EXPECT_NE(repos.service.getVehicle(1), nullptr);
}

TEST(FleetServiceUserTest, AddsGetsAndListsUsers) {
    Repositories repos;
    models::User* added = repos.service.addUser(makeCustomer(2));

    ASSERT_NE(added, nullptr);
    EXPECT_EQ(added->getUserId(), 2);
    EXPECT_EQ(repos.service.getUser(2), added);
    EXPECT_EQ(repos.service.getUser(99), nullptr);

    const auto users = repos.service.listUsers();
    ASSERT_EQ(users.size(), 1U);
    EXPECT_EQ(users.front(), added);
}

TEST(FleetServiceUserTest, RejectsNullUser) {
    Repositories repos;
    std::unique_ptr<models::User> user;
    EXPECT_THROW(repos.service.addUser(std::move(user)), core::ValidationException);
}

TEST(FleetServiceUserTest, PropagatesDuplicateUserIdException) {
    Repositories repos;
    repos.service.addUser(makeCustomer(2));
    EXPECT_THROW(repos.service.addUser(makeCustomer(2)), core::DuplicateEntityException);
    EXPECT_EQ(repos.users.size(), 1U);
}

TEST(FleetServiceUserTest, RemovesUnreferencedUser) {
    Repositories repos;
    repos.service.addUser(makeCustomer(2));
    EXPECT_NO_THROW(repos.service.removeUser(2));
    EXPECT_EQ(repos.service.getUser(2), nullptr);
}

TEST(FleetServiceUserTest, ThrowsForMissingUser) {
    Repositories repos;
    EXPECT_THROW(repos.service.removeUser(404), core::EntityNotFoundException);
}

TEST(FleetServiceUserTest, PreventsRemovalWhenRentalReferencesUser) {
    Repositories repos;
    repos.service.addUser(makeCustomer(2));
    repos.rentals.add(std::make_unique<models::RentalRecord>(
        10, 2, 1, sampleRange(), 100.0));

    EXPECT_THROW(repos.service.removeUser(2), services::BusinessException);
    EXPECT_NE(repos.service.getUser(2), nullptr);
}

TEST(FleetServiceUserTest, PreventsRemovalWhenPaymentReferencesUser) {
    Repositories repos;
    repos.service.addUser(makeCustomer(2));
    repos.payments.add(std::make_unique<models::PaymentRecord>(
        40, 10, 2, 50.0, core::Date(2026, 1, 10),
        core::PaymentType::Payment, core::PaymentMethod::UPI, "Advance payment"));

    EXPECT_THROW(repos.service.removeUser(2), services::BusinessException);
    EXPECT_NE(repos.service.getUser(2), nullptr);
}
