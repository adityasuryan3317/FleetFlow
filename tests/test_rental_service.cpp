#include "fleetflow/services/RentalService.hpp"

#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Customer.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

namespace {

using namespace fleetflow;

struct Repositories {
    storage::Repository<models::RentalRecord> rentals;
    storage::Repository<models::Vehicle> vehicles;
    storage::Repository<models::User> users;
    services::RentalService service{rentals, vehicles, users};
};

std::unique_ptr<models::Vehicle> makeCar(int id) {
    return std::make_unique<models::Car>(
        id, "Toyota", "Corolla", "CAR-" + std::to_string(id), 50.0, 5,
        "Petrol", true);
}

std::unique_ptr<models::User> makeCustomer(int id) {
    return std::make_unique<models::Customer>(
        id, "customer" + std::to_string(id), "Test Customer",
        "customer" + std::to_string(id) + "@example.com", "DL-123", "9999999999");
}

core::DateRange range(int startDay, int endDay) {
    return core::DateRange(core::Date(2026, 1, startDay), core::Date(2026, 1, endDay));
}

std::unique_ptr<models::RentalRecord> makeRental(
    int rentalId, int customerId, int vehicleId, int startDay, int endDay,
    double estimatedCost = 100.0) {
    return std::make_unique<models::RentalRecord>(
        rentalId, customerId, vehicleId, range(startDay, endDay), estimatedCost);
}

void addCustomerAndVehicle(Repositories& repos, int customerId = 7, int vehicleId = 3) {
    repos.users.add(makeCustomer(customerId));
    repos.vehicles.add(makeCar(vehicleId));
}

const core::Date today(2026, 1, 1);

} // namespace

TEST(RentalServiceBookingTest, BooksRentalWhenCustomerAndVehicleAreValid) {
    Repositories repos;
    addCustomerAndVehicle(repos);

    models::RentalRecord* created = repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today);

    ASSERT_NE(created, nullptr);
    EXPECT_EQ(created->getRentalId(), 1);
    EXPECT_EQ(created->getStatus(), core::RentalStatus::Confirmed);
    EXPECT_EQ(repos.rentals.size(), 1U);
}

TEST(RentalServiceBookingTest, RejectsNullRental) {
    Repositories repos;
    std::unique_ptr<models::RentalRecord> rental;
    EXPECT_THROW(repos.service.bookRental(std::move(rental), today), core::ValidationException);
}

TEST(RentalServiceBookingTest, RejectsMissingCustomer) {
    Repositories repos;
    repos.vehicles.add(makeCar(3));
    EXPECT_THROW(repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today),
                 core::EntityNotFoundException);
}

TEST(RentalServiceBookingTest, RejectsMissingVehicle) {
    Repositories repos;
    repos.users.add(makeCustomer(7));
    EXPECT_THROW(repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today),
                 core::EntityNotFoundException);
}

TEST(RentalServiceBookingTest, RejectsVehicleUnderMaintenance) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.vehicles.getById(3)->setStatus(core::VehicleStatus::UnderMaintenance);

    EXPECT_FALSE(repos.service.isVehicleAvailable(3, range(10, 12), today));
    EXPECT_THROW(repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today),
                 core::VehicleUnavailableException);
}

TEST(RentalServiceBookingTest, RejectsRetiredVehicle) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.vehicles.getById(3)->setStatus(core::VehicleStatus::Retired);
    EXPECT_THROW(repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today),
                 core::VehicleUnavailableException);
}

TEST(RentalServiceBookingTest, RejectsOverlappingConfirmedRental) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.rentals.add(makeRental(1, 7, 3, 10, 12));

    EXPECT_FALSE(repos.service.isVehicleAvailable(3, range(12, 14), today));
    EXPECT_THROW(repos.service.bookRental(makeRental(2, 7, 3, 12, 14), today),
                 core::VehicleUnavailableException);
}

TEST(RentalServiceBookingTest, AllowsNonOverlappingRental) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.rentals.add(makeRental(1, 7, 3, 10, 12));

    EXPECT_TRUE(repos.service.isVehicleAvailable(3, range(13, 15), today));
    EXPECT_NO_THROW(repos.service.bookRental(makeRental(2, 7, 3, 13, 15), today));
}

TEST(RentalServiceBookingTest, CancelledRentalDoesNotBlockAvailability) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    auto existing = makeRental(1, 7, 3, 10, 12);
    existing->markCancelled();
    repos.rentals.add(std::move(existing));

    EXPECT_TRUE(repos.service.isVehicleAvailable(3, range(10, 12), today));
}

TEST(RentalServiceBookingTest, RejectsNonFiniteEstimatedCost) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    auto rental = makeRental(1, 7, 3, 10, 12,
                             std::numeric_limits<double>::quiet_NaN());
    EXPECT_THROW(repos.service.bookRental(std::move(rental), today), core::ValidationException);
    EXPECT_TRUE(repos.rentals.empty());
}

TEST(RentalServiceBookingTest, RejectsDuplicateRentalId) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today);
    EXPECT_THROW(repos.service.bookRental(makeRental(1, 7, 3, 13, 15), today),
                 core::DuplicateEntityException);
}

TEST(RentalServiceAvailabilityTest, ThrowsWhenVehicleDoesNotExist) {
    Repositories repos;
    EXPECT_THROW(repos.service.isVehicleAvailable(404, range(10, 12), today),
                 core::EntityNotFoundException);
}

TEST(RentalServiceLifecycleTest, ActivatesConfirmedRental) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today);

    repos.service.activateRental(1, core::Date(2026, 1, 10));
    ASSERT_NE(repos.service.getRental(1), nullptr);
    EXPECT_EQ(repos.service.getRental(1)->getStatus(), core::RentalStatus::Active);
    EXPECT_EQ(repos.service.getRental(1)->getActualStartDate(), core::Date(2026, 1, 10));
}

TEST(RentalServiceLifecycleTest, CancelsConfirmedRental) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today);

    repos.service.cancelRental(1);
    EXPECT_EQ(repos.service.getRental(1)->getStatus(), core::RentalStatus::Cancelled);
}

TEST(RentalServiceLifecycleTest, CompletesActiveRental) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today);
    repos.service.activateRental(1, core::Date(2026, 1, 10));
    repos.service.completeRental(1, core::Date(2026, 1, 11), 125.0);

    EXPECT_EQ(repos.service.getRental(1)->getStatus(), core::RentalStatus::Completed);
    EXPECT_EQ(repos.service.getRental(1)->getFinalCost(), 125.0);
}

TEST(RentalServiceLifecycleTest, PropagatesInvalidStateTransition) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today);
    repos.service.activateRental(1, core::Date(2026, 1, 10));

    EXPECT_THROW(repos.service.cancelRental(1), core::InvalidOperationException);
}

TEST(RentalServiceLifecycleTest, ThrowsForMissingRental) {
    Repositories repos;
    EXPECT_THROW(repos.service.cancelRental(404), core::EntityNotFoundException);
    EXPECT_THROW(repos.service.activateRental(404, core::Date(2026, 1, 10)),
                 core::EntityNotFoundException);
    EXPECT_THROW(repos.service.completeRental(404, core::Date(2026, 1, 10), 10.0),
                 core::EntityNotFoundException);
}

TEST(RentalServiceQueryTest, GetsAndListsRentalsByCustomerAndVehicle) {
    Repositories repos;
    addCustomerAndVehicle(repos, 7, 3);
    repos.users.add(makeCustomer(8));
    repos.vehicles.add(makeCar(4));

    repos.service.bookRental(makeRental(1, 7, 3, 10, 12), today);
    repos.service.bookRental(makeRental(2, 8, 4, 10, 12), today);

    ASSERT_NE(repos.service.getRental(1), nullptr);
    EXPECT_EQ(repos.service.getRental(99), nullptr);
    EXPECT_EQ(repos.service.listRentals().size(), 2U);
    EXPECT_EQ(repos.service.listRentalsForCustomer(7).size(), 1U);
    EXPECT_EQ(repos.service.listRentalsForVehicle(4).size(), 1U);
}

TEST(RentalServiceAvailabilityTest, ActiveRentalBlocksOverlappingAndLaterWindows) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    auto active = makeRental(1, 7, 3, 10, 12);
    active->markActive(core::Date(2026, 1, 10));
    repos.rentals.add(std::move(active));

    EXPECT_FALSE(repos.service.isVehicleAvailable(3, range(11, 11), today));
    EXPECT_FALSE(repos.service.isVehicleAvailable(3, range(15, 16), today));
}


TEST(RentalServiceAvailabilityTest, DelayedPickupUsesActualStartForVehicleOccupancy) {
    Repositories repos;
    addCustomerAndVehicle(repos);

    auto delayed = makeRental(1, 7, 3, 10, 15);
    delayed->markActive(core::Date(2026, 1, 12));
    repos.rentals.add(std::move(delayed));

    // Once pickup happens late, unused scheduled days before Jan 12 are available again.
    EXPECT_TRUE(repos.service.isVehicleAvailable(3, range(10, 11), today));
    EXPECT_FALSE(repos.service.isVehicleAvailable(3, range(11, 12), today));
    EXPECT_FALSE(repos.service.isVehicleAvailable(3, range(13, 14), today));

    repos.service.completeRental(1, core::Date(2026, 1, 14), 80.0);

    // Completed occupancy is the actual interval [Jan 12, Jan 14], not [Jan 10, Jan 14].
    EXPECT_TRUE(repos.service.isVehicleAvailable(3, range(10, 11), today));
    EXPECT_FALSE(repos.service.isVehicleAvailable(3, range(12, 14), today));
    EXPECT_TRUE(repos.service.isVehicleAvailable(3, range(15, 16), today));
}

TEST(RentalServiceAvailabilityTest, CompletedEarlyReturnReleasesLaterScheduledDays) {
    Repositories repos;
    addCustomerAndVehicle(repos);
    auto completed = makeRental(1, 7, 3, 10, 15);
    completed->markActive(core::Date(2026, 1, 10));
    completed->completeRental(core::Date(2026, 1, 12), 80.0);
    repos.rentals.add(std::move(completed));

    EXPECT_FALSE(repos.service.isVehicleAvailable(3, range(11, 12), today));
    EXPECT_TRUE(repos.service.isVehicleAvailable(3, range(13, 15), today));
}
