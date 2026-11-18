#include "fleetflow/models/MaintenanceRecord.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <cmath>
#include <iomanip>
#include <ostream>

namespace fleetflow::models {

namespace {

bool isBlank(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

} // anonymous namespace

MaintenanceRecord::MaintenanceRecord(int mId,
                                     int vId,
                                     std::string desc,
                                     core::Date start,
                                     double estimatedCost)
    : startDate(start) {

    if (mId <= 0) {
        throw core::ValidationException("Maintenance ID must be a positive integer, received: " + std::to_string(mId));
    }
    if (vId <= 0) {
        throw core::ValidationException("Vehicle ID must be a positive integer, received: " + std::to_string(vId));
    }
    if (isBlank(desc)) {
        throw core::ValidationException("Maintenance problem description cannot be empty or whitespace.");
    }
    if (estimatedCost < 0.0) {
        throw core::ValidationException("Estimated maintenance cost cannot be negative, received: " + std::to_string(estimatedCost));
    }

    maintenanceId = mId;
    vehicleId = vId;
    problemDescription = std::move(desc);
    cost = estimatedCost;
    status = core::MaintenanceStatus::Scheduled;
}

void MaintenanceRecord::startMaintenance() {
    if (status != core::MaintenanceStatus::Scheduled) {
        throw core::InvalidOperationException(
            "Cannot start maintenance #" + std::to_string(maintenanceId) +
            ": current status is " + core::toString(status) + ", expected Scheduled.");
    }
    status = core::MaintenanceStatus::InProgress;
}

void MaintenanceRecord::completeMaintenance(const core::Date& finishDate, double actualCost) {
    if (status != core::MaintenanceStatus::InProgress) {
        throw core::InvalidOperationException(
            "Cannot complete maintenance #" + std::to_string(maintenanceId) +
            ": current status is " + core::toString(status) + ", expected InProgress.");
    }
    if (finishDate < startDate) {
        throw core::InvalidDateRangeException(
            "Maintenance completion date (" + finishDate.toString() +
            ") cannot precede start date (" + startDate.toString() + ").");
    }
    if (actualCost < 0.0) {
        throw core::ValidationException(
            "Final maintenance cost cannot be negative, received: " + std::to_string(actualCost));
    }

    completionDate = finishDate;
    cost = actualCost;
    status = core::MaintenanceStatus::Completed;
}

void MaintenanceRecord::updateCost(double newCost) {
    if (newCost < 0.0) {
        throw core::ValidationException(
            "Maintenance cost cannot be negative, received: " + std::to_string(newCost));
    }
    cost = newCost;
}

MaintenanceRecord MaintenanceRecord::restoreFromStorage(
    int mId,
    int vId,
    std::string desc,
    core::Date start,
    std::optional<core::Date> completion,
    double actualCost,
    core::MaintenanceStatus stat) {

    if (mId <= 0) {
        throw core::StorageException("Corrupted MaintenanceRecord: maintenanceId must be positive, received: " + std::to_string(mId));
    }
    if (vId <= 0) {
        throw core::StorageException("Corrupted MaintenanceRecord: vehicleId must be positive, received: " + std::to_string(vId));
    }
    if (desc.find_first_not_of(" \t\n\r") == std::string::npos) {
        throw core::StorageException("Corrupted MaintenanceRecord: problemDescription cannot be empty or whitespace.");
    }
    if (actualCost < 0.0 || !std::isfinite(actualCost)) {
        throw core::StorageException("Corrupted MaintenanceRecord: cost must be non-negative and finite.");
    }

    if (stat == core::MaintenanceStatus::Scheduled ||
        stat == core::MaintenanceStatus::InProgress) {
        if (completion.has_value()) {
            throw core::StorageException(
                "Corrupted MaintenanceRecord #" + std::to_string(mId) +
                ": " + core::toString(stat) + " record cannot have a completion date.");
        }
    } else if (stat == core::MaintenanceStatus::Completed) {
        if (!completion.has_value()) {
            throw core::StorageException(
                "Corrupted MaintenanceRecord #" + std::to_string(mId) +
                ": Completed maintenance record must contain a valid completion date.");
        }
        if (*completion < start) {
            throw core::StorageException(
                "Corrupted MaintenanceRecord #" + std::to_string(mId) +
                ": Completion date (" + completion->toString() +
                ") precedes maintenance start date (" + start.toString() + ").");
        }
    }

    MaintenanceRecord record(mId, vId, std::move(desc), start, actualCost);
    record.status = stat;
    record.completionDate = completion;
    record.cost = actualCost;
    return record;
}

void MaintenanceRecord::displayDetails(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [MAINTENANCE RECORD #" << maintenanceId << "]\n"
       << " Vehicle ID      : " << vehicleId << "\n"
       << " Issue / Service : " << problemDescription << "\n"
       << " Start Date      : " << startDate.toString() << "\n"
       << " Completion Date : " << (completionDate ? completionDate->toString() : "In Workshop / Pending") << "\n"
       << " Servicing Cost  : ₹" << std::fixed << std::setprecision(2) << cost << "\n"
       << " Status          : " << core::toString(status) << "\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

