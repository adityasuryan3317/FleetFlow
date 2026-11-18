#include "fleetflow/services/MaintenanceService.hpp"

#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/services/Exceptions.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace fleetflow::services {

namespace {

bool isValidMaintenanceStatus(core::MaintenanceStatus status) noexcept {
    return status == core::MaintenanceStatus::Scheduled ||
           status == core::MaintenanceStatus::InProgress ||
           status == core::MaintenanceStatus::Completed;
}

void validateFiniteNonNegativeCost(double cost, const std::string& label) {
    if (!std::isfinite(cost) || cost < 0.0) {
        throw core::ValidationException(label + " must be finite and non-negative.");
    }
}

} // namespace

MaintenanceService::MaintenanceService(
    storage::Repository<models::MaintenanceRecord>& mRepo,
    storage::Repository<models::Vehicle>& vRepo,
    storage::Repository<models::RentalRecord>& rRepo) noexcept
    : maintenanceRepo(mRepo), vehicleRepo(vRepo), rentalRepo(rRepo) {}

models::MaintenanceRecord& MaintenanceService::requireMaintenance(int maintenanceId) const {
    models::MaintenanceRecord* maintenance = maintenanceRepo.getById(maintenanceId);
    if (maintenance == nullptr) {
        throw core::EntityNotFoundException(
            "Maintenance record #" + std::to_string(maintenanceId) + " not found.");
    }
    return *maintenance;
}

models::Vehicle& MaintenanceService::requireVehicle(int vehicleId) const {
    models::Vehicle* vehicle = vehicleRepo.getById(vehicleId);
    if (vehicle == nullptr) {
        throw core::EntityNotFoundException(
            "Vehicle #" + std::to_string(vehicleId) + " not found.");
    }
    return *vehicle;
}

bool MaintenanceService::hasActiveRental(int vehicleId) const noexcept {
    for (const models::RentalRecord* rental : rentalRepo.getAll()) {
        if (rental->getVehicleId() == vehicleId &&
            rental->getStatus() == core::RentalStatus::Active) {
            return true;
        }
    }
    return false;
}

bool MaintenanceService::hasOtherInProgressMaintenance(
    int vehicleId, int exceptMaintenanceId) const noexcept {
    for (const models::MaintenanceRecord* maintenance : maintenanceRepo.getAll()) {
        if (maintenance->getMaintenanceId() != exceptMaintenanceId &&
            maintenance->getVehicleId() == vehicleId &&
            maintenance->getStatus() == core::MaintenanceStatus::InProgress) {
            return true;
        }
    }
    return false;
}

models::MaintenanceRecord* MaintenanceService::scheduleMaintenance(
    std::unique_ptr<models::MaintenanceRecord> maintenance) {
    if (!maintenance) {
        throw core::ValidationException("Cannot schedule a null maintenance record.");
    }

    const int maintenanceId = maintenance->getMaintenanceId();
    const int vehicleId = maintenance->getVehicleId();

    if (maintenanceRepo.exists(maintenanceId)) {
        throw core::DuplicateEntityException(
            "Maintenance record with ID #" + std::to_string(maintenanceId) +
            " already exists.");
    }

    models::Vehicle& vehicle = requireVehicle(vehicleId);
    if (vehicle.getStatus() == core::VehicleStatus::Retired) {
        throw core::VehicleUnavailableException(
            "Cannot schedule maintenance for retired vehicle #" +
            std::to_string(vehicleId) + ".");
    }

    validateFiniteNonNegativeCost(maintenance->getCost(), "Estimated maintenance cost");

    if (maintenance->getStatus() != core::MaintenanceStatus::Scheduled) {
        throw core::ValidationException(
            "Only a Scheduled maintenance record can be submitted for scheduling.");
    }

    maintenanceRepo.add(std::move(maintenance));
    return maintenanceRepo.getById(maintenanceId);
}

void MaintenanceService::startMaintenance(int maintenanceId) {
    models::MaintenanceRecord& maintenance = requireMaintenance(maintenanceId);
    models::Vehicle& vehicle = requireVehicle(maintenance.getVehicleId());

    if (vehicle.getStatus() == core::VehicleStatus::Retired) {
        throw core::VehicleUnavailableException(
            "Cannot start maintenance on retired vehicle #" +
            std::to_string(vehicle.getVehicleId()) + ".");
    }

    if (hasActiveRental(vehicle.getVehicleId())) {
        throw core::VehicleUnavailableException(
            "Cannot start maintenance while vehicle #" +
            std::to_string(vehicle.getVehicleId()) + " has an active rental.");
    }

    if (hasOtherInProgressMaintenance(vehicle.getVehicleId(), maintenanceId)) {
        throw BusinessException(
            "Vehicle #" + std::to_string(vehicle.getVehicleId()) +
            " already has another maintenance record in progress.");
    }

    // Let MaintenanceRecord validate the lifecycle transition before changing
    // the vehicle status, so a failed transition cannot leave status inconsistent.
    maintenance.startMaintenance();
    vehicle.setStatus(core::VehicleStatus::UnderMaintenance);
}

void MaintenanceService::completeMaintenance(
    int maintenanceId, const core::Date& finishDate, double actualCost) {
    models::MaintenanceRecord& maintenance = requireMaintenance(maintenanceId);
    models::Vehicle& vehicle = requireVehicle(maintenance.getVehicleId());

    validateFiniteNonNegativeCost(actualCost, "Actual maintenance cost");

    // The model validates status and completion-date ordering.
    maintenance.completeMaintenance(finishDate, actualCost);

    if (vehicle.getStatus() == core::VehicleStatus::UnderMaintenance &&
        !hasOtherInProgressMaintenance(vehicle.getVehicleId(), maintenanceId)) {
        // Retired vehicles are deliberately not reactivated; the guard above
        // only changes the explicit UnderMaintenance state.
        vehicle.setStatus(core::VehicleStatus::Available);
    }
}

void MaintenanceService::updateMaintenanceCost(int maintenanceId, double newCost) {
    models::MaintenanceRecord& maintenance = requireMaintenance(maintenanceId);
    validateFiniteNonNegativeCost(newCost, "Maintenance cost");
    maintenance.updateCost(newCost);
}

models::MaintenanceRecord* MaintenanceService::getMaintenance(
    int maintenanceId) const noexcept {
    return maintenanceRepo.getById(maintenanceId);
}

std::vector<models::MaintenanceRecord*> MaintenanceService::listMaintenance() const {
    return maintenanceRepo.getAll();
}

std::vector<models::MaintenanceRecord*> MaintenanceService::listMaintenanceForVehicle(
    int vehicleId) const {
    (void)requireVehicle(vehicleId);

    std::vector<models::MaintenanceRecord*> result;
    for (models::MaintenanceRecord* maintenance : maintenanceRepo.getAll()) {
        if (maintenance->getVehicleId() == vehicleId) {
            result.push_back(maintenance);
        }
    }
    return result;
}

std::vector<models::MaintenanceRecord*> MaintenanceService::listMaintenanceByStatus(
    core::MaintenanceStatus status) const {
    if (!isValidMaintenanceStatus(status)) {
        throw core::ValidationException("Maintenance status is invalid.");
    }

    std::vector<models::MaintenanceRecord*> result;
    for (models::MaintenanceRecord* maintenance : maintenanceRepo.getAll()) {
        if (maintenance->getStatus() == status) {
            result.push_back(maintenance);
        }
    }
    return result;
}

bool MaintenanceService::isVehicleUnderMaintenance(int vehicleId) const {
    return requireVehicle(vehicleId).getStatus() == core::VehicleStatus::UnderMaintenance;
}

} // namespace fleetflow::services
