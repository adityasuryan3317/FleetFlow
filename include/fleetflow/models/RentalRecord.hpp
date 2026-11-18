#pragma once

#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/DateRange.hpp"

#include <optional>
#include <cstring>
#include <iosfwd>

namespace fleetflow::models {

/**
 * @brief Represents a customer booking contract and vehicle reservation lifecycle.
 * Manages interval scheduling, checkout/in states, and occupancy conflict resolution.
 */
class RentalRecord {
private:
    int rentalId{0};
    int customerId{0};
    int vehicleId{0};
    core::DateRange scheduledRange;                // scheduled start & end dates
    std::optional<core::Date> actualStartDate;    // actual activation date (optional)
    std::optional<core::Date> actualReturnDate;   // set only on completion
    double estimatedCost{0.0};
    std::optional<double> finalCost;              // set only on completion
    core::RentalStatus status{core::RentalStatus::Confirmed};

public:
    /**
     * @brief Constructs a new reservation. Every rental starts strictly as Confirmed.
     */
    RentalRecord(int rId,
                 int cId,
                 int vId,
                 core::DateRange range,
                 double estCost);

    // Accessors
    int getRentalId() const noexcept { return rentalId; }
    int getCustomerId() const noexcept { return customerId; }
    int getVehicleId() const noexcept { return vehicleId; }
    const core::DateRange& getScheduledRange() const noexcept { return scheduledRange; }
    const std::optional<core::Date>& getActualStartDate() const noexcept { return actualStartDate; }
    const std::optional<core::Date>& getActualReturnDate() const noexcept { return actualReturnDate; }
    double getEstimatedCost() const noexcept { return estimatedCost; }
    const std::optional<double>& getFinalCost() const noexcept { return finalCost; }
    core::RentalStatus getStatus() const noexcept { return status; }

    // Controlled State Machine Transitions
    void markActive(const core::Date& startDate);
    void markCancelled();
    void completeRental(const core::Date& returnDate, double finalSettledCost);

    /**
     * @brief Restores a RentalRecord from persistent storage with complete invariant verification.
     * @throws core::StorageException if status and dates/costs are in contradictory states.
     */
    static RentalRecord restoreFromStorage(
        int rId,
        int cId,
        int vId,
        core::DateRange range,
        std::optional<core::Date> actualStart,
        std::optional<core::Date> actualReturn,
        double estCost,
        std::optional<double> finCost,
        core::RentalStatus stat);

    /**
     * @brief Determines whether this rental record blocks a vehicle for a requested booking window.
     * @param requestedRange The requested interval to be booked.
     * @param today Current system reference date (used to detect overdue active rentals).
     * @return true if there is a scheduling conflict, false otherwise.
     */
    bool conflictsWith(const core::DateRange& requestedRange, const core::Date& today) const;

    void displayDetails(std::ostream& os) const;
};

} // namespace fleetflow::models
