#pragma once

#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Date.hpp"

#include <optional>
#include <string>
#include <iosfwd>

namespace fleetflow::models {

/**
 * @brief Represents a scheduled, ongoing, or completed mechanical servicing record.
 * Manages maintenance lifecycle transitions and tracks servicing expenditures.
 */
class MaintenanceRecord {
private:
    int maintenanceId{0};
    int vehicleId{0};
    std::string problemDescription;
    core::Date startDate;
    std::optional<core::Date> completionDate;
    double cost{0.0};
    core::MaintenanceStatus status{core::MaintenanceStatus::Scheduled};

public:
    /**
     * @brief Constructs a new maintenance record. Every record begins strictly as Scheduled.
     */
    MaintenanceRecord(int mId,
                      int vId,
                      std::string desc,
                      core::Date start,
                      double estimatedCost = 0.0);

    // Accessors
    int getMaintenanceId() const noexcept { return maintenanceId; }
    int getVehicleId() const noexcept { return vehicleId; }
    const std::string& getProblemDescription() const noexcept { return problemDescription; }
    const core::Date& getStartDate() const noexcept { return startDate; }
    const std::optional<core::Date>& getCompletionDate() const noexcept { return completionDate; }
    double getCost() const noexcept { return cost; }
    core::MaintenanceStatus getStatus() const noexcept { return status; }

    // Controlled State Transitions
    void startMaintenance();
    void completeMaintenance(const core::Date& finishDate, double actualCost);
    void updateCost(double newCost);

    /**
     * @brief Restores a MaintenanceRecord from persistent storage with complete invariant verification.
     * @throws core::StorageException if status and dates/costs are in contradictory states.
     */
    static MaintenanceRecord restoreFromStorage(
        int mId,
        int vId,
        std::string desc,
        core::Date start,
        std::optional<core::Date> completion,
        double actualCost,
        core::MaintenanceStatus stat);

    void displayDetails(std::ostream& os) const;
};

} // namespace fleetflow::models

