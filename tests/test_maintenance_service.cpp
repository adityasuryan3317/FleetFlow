#include "fleetflow/services/MaintenanceService.hpp"

#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/services/Exceptions.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <string>

namespace {
using namespace fleetflow;

struct Repositories {
    storage::Repository<models::MaintenanceRecord> maintenance;
    storage::Repository<models::Vehicle> vehicles;
    storage::Repository<models::RentalRecord> rentals;
    services::MaintenanceService service{maintenance, vehicles, rentals};
};

std::unique_ptr<models::Vehicle> makeCar(
    int id, core::VehicleStatus status = core::VehicleStatus::Available) {
    return std::make_unique<models::Car>(
        id, "Toyota", "Corolla", "CAR-" + std::to_string(id),
        50.0, 5, "Petrol", true, status);
}

std::unique_ptr<models::MaintenanceRecord> makeMaintenance(
    int id, int vehicleId, double cost = 250.0) {
    return std::make_unique<models::MaintenanceRecord>(
        id, vehicleId, "Routine service", core::Date(2026, 2, 10), cost);
}

std::unique_ptr<models::RentalRecord> makeActiveRental(int id, int vehicleId) {
    auto rental = std::make_unique<models::RentalRecord>(
        id, 7, vehicleId,
        core::DateRange(core::Date(2026, 2, 1), core::Date(2026, 2, 20)),
        500.0);
    rental->markActive(core::Date(2026, 2, 1));
    return rental;
}

void addVehicle(Repositories& repos, int vehicleId = 3,
                core::VehicleStatus status = core::VehicleStatus::Available) {
    repos.vehicles.add(makeCar(vehicleId, status));
}
} // namespace

TEST(MaintenanceServiceSchedulingTest, SchedulesMaintenanceForExistingVehicle) {
    Repositories repos;
    addVehicle(repos);
    auto* created = repos.service.scheduleMaintenance(makeMaintenance(1, 3));
    ASSERT_NE(created, nullptr);
    EXPECT_EQ(created->getMaintenanceId(), 1);
    EXPECT_EQ(created->getStatus(), core::MaintenanceStatus::Scheduled);
    EXPECT_EQ(repos.maintenance.size(), 1U);
}

TEST(MaintenanceServiceSchedulingTest, RejectsNullRecord) {
    Repositories repos;
    std::unique_ptr<models::MaintenanceRecord> record;
    EXPECT_THROW(repos.service.scheduleMaintenance(std::move(record)),
                 core::ValidationException);
}

TEST(MaintenanceServiceSchedulingTest, RejectsMissingVehicle) {
    Repositories repos;
    EXPECT_THROW(repos.service.scheduleMaintenance(makeMaintenance(1, 404)),
                 core::EntityNotFoundException);
    EXPECT_TRUE(repos.maintenance.empty());
}

TEST(MaintenanceServiceSchedulingTest, RejectsRetiredVehicle) {
    Repositories repos;
    addVehicle(repos, 3, core::VehicleStatus::Retired);
    EXPECT_THROW(repos.service.scheduleMaintenance(makeMaintenance(1, 3)),
                 core::VehicleUnavailableException);
    EXPECT_TRUE(repos.maintenance.empty());
}

TEST(MaintenanceServiceSchedulingTest, RejectsDuplicateMaintenanceId) {
    Repositories repos;
    addVehicle(repos);
    repos.service.scheduleMaintenance(makeMaintenance(1, 3));
    EXPECT_THROW(repos.service.scheduleMaintenance(makeMaintenance(1, 3)),
                 core::DuplicateEntityException);
    EXPECT_EQ(repos.maintenance.size(), 1U);
}

TEST(MaintenanceServiceSchedulingTest, RejectsNonFiniteEstimatedCost) {
    Repositories repos;
    addVehicle(repos);
    auto record = makeMaintenance(1, 3, std::numeric_limits<double>::quiet_NaN());
    EXPECT_THROW(repos.service.scheduleMaintenance(std::move(record)),
                 core::ValidationException);
    EXPECT_TRUE(repos.maintenance.empty());
}

TEST(MaintenanceServiceSchedulingTest, SchedulingDoesNotImmediatelyDisableVehicle) {
    Repositories repos;
    addVehicle(repos);
    repos.service.scheduleMaintenance(makeMaintenance(1, 3));
    EXPECT_EQ(repos.vehicles.getById(3)->getStatus(), core::VehicleStatus::Available);
    EXPECT_FALSE(repos.service.isVehicleUnderMaintenance(3));
}

TEST(MaintenanceServiceLifecycleTest, StartsMaintenanceAndMarksVehicleUnavailable) {
    Repositories repos;
    addVehicle(repos);
    repos.service.scheduleMaintenance(makeMaintenance(1, 3));
    repos.service.startMaintenance(1);
    EXPECT_EQ(repos.service.getMaintenance(1)->getStatus(),
              core::MaintenanceStatus::InProgress);
    EXPECT_EQ(repos.vehicles.getById(3)->getStatus(),
              core::VehicleStatus::UnderMaintenance);
    EXPECT_TRUE(repos.service.isVehicleUnderMaintenance(3));
}

TEST(MaintenanceServiceLifecycleTest, ThrowsWhenStartingMissingRecord) {
    Repositories repos;
    EXPECT_THROW(repos.service.startMaintenance(404), core::EntityNotFoundException);
}

TEST(MaintenanceServiceLifecycleTest, ThrowsWhenStartingRecordWithMissingVehicle) {
    Repositories repos;
    repos.maintenance.add(makeMaintenance(1, 404));
    EXPECT_THROW(repos.service.startMaintenance(1), core::EntityNotFoundException);
    EXPECT_EQ(repos.service.getMaintenance(1)->getStatus(),
              core::MaintenanceStatus::Scheduled);
}

TEST(MaintenanceServiceLifecycleTest, CannotStartMaintenanceDuringActiveRental) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    repos.rentals.add(makeActiveRental(10, 3));
    EXPECT_THROW(repos.service.startMaintenance(1), core::VehicleUnavailableException);
    EXPECT_EQ(repos.service.getMaintenance(1)->getStatus(),
              core::MaintenanceStatus::Scheduled);
    EXPECT_EQ(repos.vehicles.getById(3)->getStatus(), core::VehicleStatus::Available);
}

TEST(MaintenanceServiceLifecycleTest, CannotStartTwoMaintenanceJobsConcurrentlyForVehicle) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    repos.maintenance.add(makeMaintenance(2, 3));
    repos.service.startMaintenance(1);
    EXPECT_THROW(repos.service.startMaintenance(2), services::BusinessException);
    EXPECT_EQ(repos.service.getMaintenance(2)->getStatus(),
              core::MaintenanceStatus::Scheduled);
}

TEST(MaintenanceServiceLifecycleTest, CannotStartMaintenanceForRetiredVehicle) {
    Repositories repos;
    addVehicle(repos, 3, core::VehicleStatus::Retired);
    repos.maintenance.add(makeMaintenance(1, 3));
    EXPECT_THROW(repos.service.startMaintenance(1), core::VehicleUnavailableException);
}

TEST(MaintenanceServiceLifecycleTest, CompletesMaintenanceAndRestoresVehicleAvailability) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    repos.service.startMaintenance(1);
    repos.service.completeMaintenance(1, core::Date(2026, 2, 12), 875.50);
    EXPECT_EQ(repos.service.getMaintenance(1)->getStatus(),
              core::MaintenanceStatus::Completed);
    EXPECT_EQ(repos.service.getMaintenance(1)->getCompletionDate(),
              core::Date(2026, 2, 12));
    EXPECT_DOUBLE_EQ(repos.service.getMaintenance(1)->getCost(), 875.50);
    EXPECT_EQ(repos.vehicles.getById(3)->getStatus(), core::VehicleStatus::Available);
}

TEST(MaintenanceServiceLifecycleTest, PreservesUnderMaintenanceWhenAnotherJobIsInProgress) {
    Repositories repos;
    addVehicle(repos);
    auto first = makeMaintenance(1, 3);
    auto second = makeMaintenance(2, 3);
    first->startMaintenance();
    second->startMaintenance();
    repos.maintenance.add(std::move(first));
    repos.maintenance.add(std::move(second));
    repos.vehicles.getById(3)->setStatus(core::VehicleStatus::UnderMaintenance);
    repos.service.completeMaintenance(1, core::Date(2026, 2, 12), 500.0);
    EXPECT_EQ(repos.service.getMaintenance(1)->getStatus(),
              core::MaintenanceStatus::Completed);
    EXPECT_EQ(repos.service.getMaintenance(2)->getStatus(),
              core::MaintenanceStatus::InProgress);
    EXPECT_EQ(repos.vehicles.getById(3)->getStatus(),
              core::VehicleStatus::UnderMaintenance);
}

TEST(MaintenanceServiceLifecycleTest, RejectsNonFiniteActualCostWithoutCompletingRecord) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    repos.service.startMaintenance(1);
    EXPECT_THROW(
        repos.service.completeMaintenance(
            1, core::Date(2026, 2, 12),
            std::numeric_limits<double>::infinity()),
        core::ValidationException);
    EXPECT_EQ(repos.service.getMaintenance(1)->getStatus(),
              core::MaintenanceStatus::InProgress);
    EXPECT_EQ(repos.vehicles.getById(3)->getStatus(),
              core::VehicleStatus::UnderMaintenance);
}

TEST(MaintenanceServiceLifecycleTest, PropagatesInvalidModelStateTransition) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    EXPECT_THROW(
        repos.service.completeMaintenance(1, core::Date(2026, 2, 12), 500.0),
        core::InvalidOperationException);
    EXPECT_EQ(repos.vehicles.getById(3)->getStatus(), core::VehicleStatus::Available);
}

TEST(MaintenanceServiceLifecycleTest, PropagatesCompletionDateValidation) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    repos.service.startMaintenance(1);
    EXPECT_THROW(
        repos.service.completeMaintenance(1, core::Date(2026, 2, 9), 500.0),
        core::InvalidDateRangeException);
    EXPECT_EQ(repos.service.getMaintenance(1)->getStatus(),
              core::MaintenanceStatus::InProgress);
    EXPECT_EQ(repos.vehicles.getById(3)->getStatus(),
              core::VehicleStatus::UnderMaintenance);
}

TEST(MaintenanceServiceLifecycleTest, ThrowsWhenCompletingMissingRecord) {
    Repositories repos;
    EXPECT_THROW(
        repos.service.completeMaintenance(404, core::Date(2026, 2, 12), 100.0),
        core::EntityNotFoundException);
}

TEST(MaintenanceServiceUpdateTest, UpdatesMaintenanceCost) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    repos.service.updateMaintenanceCost(1, 475.25);
    EXPECT_DOUBLE_EQ(repos.service.getMaintenance(1)->getCost(), 475.25);
}

TEST(MaintenanceServiceUpdateTest, RejectsNonFiniteUpdatedCost) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    EXPECT_THROW(
        repos.service.updateMaintenanceCost(
            1, std::numeric_limits<double>::quiet_NaN()),
        core::ValidationException);
    EXPECT_DOUBLE_EQ(repos.service.getMaintenance(1)->getCost(), 250.0);
}

TEST(MaintenanceServiceQueryTest, GetsAndListsMaintenanceRecords) {
    Repositories repos;
    addVehicle(repos, 3);
    addVehicle(repos, 4);
    repos.maintenance.add(makeMaintenance(1, 3));
    repos.maintenance.add(makeMaintenance(2, 4));
    EXPECT_NE(repos.service.getMaintenance(1), nullptr);
    EXPECT_EQ(repos.service.getMaintenance(99), nullptr);
    EXPECT_EQ(repos.service.listMaintenance().size(), 2U);
    EXPECT_EQ(repos.service.listMaintenanceForVehicle(3).size(), 1U);
    EXPECT_EQ(repos.service.listMaintenanceForVehicle(4).size(), 1U);
    EXPECT_THROW(repos.service.listMaintenanceForVehicle(999),
                 core::EntityNotFoundException);
}

TEST(MaintenanceServiceQueryTest, FiltersMaintenanceRecordsByStatus) {
    Repositories repos;
    addVehicle(repos);
    repos.maintenance.add(makeMaintenance(1, 3));
    auto inProgress = makeMaintenance(2, 3);
    inProgress->startMaintenance();
    repos.maintenance.add(std::move(inProgress));
    EXPECT_EQ(repos.service.listMaintenanceByStatus(
                  core::MaintenanceStatus::Scheduled).size(), 1U);
    EXPECT_EQ(repos.service.listMaintenanceByStatus(
                  core::MaintenanceStatus::InProgress).size(), 1U);
    EXPECT_EQ(repos.service.listMaintenanceByStatus(
                  core::MaintenanceStatus::Completed).size(), 0U);
    EXPECT_THROW(repos.service.listMaintenanceByStatus(
                     static_cast<core::MaintenanceStatus>(999)),
                 core::ValidationException);
}

TEST(MaintenanceServiceQueryTest, UnderMaintenanceQueryRejectsMissingVehicle) {
    Repositories repos;
    EXPECT_THROW(repos.service.isVehicleUnderMaintenance(404),
                 core::EntityNotFoundException);
}
