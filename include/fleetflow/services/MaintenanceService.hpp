#pragma once

#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Date.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/storage/Repository.hpp"

#include <memory>
#include <vector>

namespace fleetflow::services {

/**
 * @brief Coordinates vehicle maintenance records and vehicle operational status.
 *
 * MaintenanceRecord remains the source of truth for its Scheduled -> InProgress
 * -> Completed state machine. MaintenanceService validates repository references,
 * prevents a vehicle from entering maintenance while it has an active rental, and
 * synchronizes VehicleStatus with in-progress maintenance.
 *
 * Policy encoded here:
 * - Scheduling a record does not immediately take a vehicle out of service.
 * - Starting maintenance marks the vehicle UnderMaintenance.
 * - A vehicle cannot start a second in-progress maintenance record concurrently.
 * - Completing the last in-progress record restores an UnderMaintenance vehicle
 *   to Available. A Retired vehicle is never reactivated.
 *
 * These are application-service policies inferred from the existing domain model;
 * they do not add date-window scheduling because MaintenanceRecord has no planned
 * end date. Returned pointers are non-owning and follow Repository's lifetime rules.
 */
class MaintenanceService {
private:
    storage::Repository<models::MaintenanceRecord>& maintenanceRepo;
    storage::Repository<models::Vehicle>& vehicleRepo;
    storage::Repository<models::RentalRecord>& rentalRepo;

    models::MaintenanceRecord& requireMaintenance(int maintenanceId) const;
    models::Vehicle& requireVehicle(int vehicleId) const;
    bool hasActiveRental(int vehicleId) const noexcept;
    bool hasOtherInProgressMaintenance(int vehicleId, int exceptMaintenanceId) const noexcept;

public:
    MaintenanceService(
        storage::Repository<models::MaintenanceRecord>& mRepo,
        storage::Repository<models::Vehicle>& vRepo,
        storage::Repository<models::RentalRecord>& rRepo) noexcept;

    /**
     * @brief Adds a new Scheduled maintenance record for an existing, non-retired vehicle.
     * @param maintenance Record to schedule; ownership transfers on success.
     * @throws core::ValidationException for null records, non-finite costs, or non-Scheduled records.
     * @throws core::EntityNotFoundException if the referenced vehicle is absent.
     * @throws core::VehicleUnavailableException if the vehicle is Retired.
     * @throws core::DuplicateEntityException if the maintenance ID already exists.
     */
    models::MaintenanceRecord* scheduleMaintenance(
        std::unique_ptr<models::MaintenanceRecord> maintenance);

    /** @brief Starts a scheduled record and marks its vehicle UnderMaintenance. */
    void startMaintenance(int maintenanceId);

    /**
     * @brief Completes an in-progress record; the vehicle becomes Available when
     *        no other maintenance record for it remains InProgress.
     */
    void completeMaintenance(int maintenanceId,
                             const core::Date& finishDate,
                             double actualCost);

    /** @brief Updates the estimated/current cost using a finite non-negative amount. */
    void updateMaintenanceCost(int maintenanceId, double newCost);

    /** @brief Direct lookup returns nullptr when the ID is absent. */
    models::MaintenanceRecord* getMaintenance(int maintenanceId) const noexcept;

    std::vector<models::MaintenanceRecord*> listMaintenance() const;

    /** @brief Requires the vehicle to exist, then returns its maintenance history. */
    std::vector<models::MaintenanceRecord*> listMaintenanceForVehicle(int vehicleId) const;

    /** @brief Returns records with the requested lifecycle status. */
    std::vector<models::MaintenanceRecord*> listMaintenanceByStatus(
        core::MaintenanceStatus status) const;

    /** @brief Requires the vehicle to exist and reports its operational status. */
    bool isVehicleUnderMaintenance(int vehicleId) const;
};

} // namespace fleetflow::services
