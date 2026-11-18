#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <cmath>
#include <iomanip>
#include <ostream>

namespace fleetflow::models {

RentalRecord::RentalRecord(int rId,
                           int cId,
                           int vId,
                           core::DateRange range,
                           double estCost)
    : scheduledRange(range) {

    if (rId <= 0) {
        throw core::ValidationException("Rental ID must be a positive integer, received: " + std::to_string(rId));
    }
    if (cId <= 0) {
        throw core::ValidationException("Customer ID must be a positive integer, received: " + std::to_string(cId));
    }
    if (vId <= 0) {
        throw core::ValidationException("Vehicle ID must be a positive integer, received: " + std::to_string(vId));
    }
    if (estCost < 0.0) {
        throw core::ValidationException("Estimated rental cost cannot be negative, received: " + std::to_string(estCost));
    }

    rentalId = rId;
    customerId = cId;
    vehicleId = vId;
    estimatedCost = estCost;
    status = core::RentalStatus::Confirmed;
}

void RentalRecord::markActive(const core::Date& startDate) {
    if (status != core::RentalStatus::Confirmed) {
        throw core::InvalidOperationException(
            "Cannot activate rental #" + std::to_string(rentalId) +
            ": current status is " + core::toString(status) + ", expected Confirmed.");
    }
    if (startDate < scheduledRange.getStartDate()) {
        throw core::InvalidDateRangeException(
            "Actual start date (" + startDate.toString() + ") cannot precede scheduled start date (" + scheduledRange.getStartDate().toString() + ")");
    }
    actualStartDate = startDate;
    status = core::RentalStatus::Active;
}

void RentalRecord::markCancelled() {
    if (status != core::RentalStatus::Confirmed) {
        throw core::InvalidOperationException(
            "Cannot cancel rental #" + std::to_string(rentalId) +
            ": current status is " + core::toString(status) + ", expected Confirmed.");
    }
    status = core::RentalStatus::Cancelled;
}

void RentalRecord::completeRental(const core::Date& returnDate, double finalSettledCost) {
    if (status != core::RentalStatus::Active) {
        throw core::InvalidOperationException(
            "Cannot check in / complete rental #" + std::to_string(rentalId) +
            ": current status is " + core::toString(status) + ", expected Active.");
    }
    const core::Date& effectiveStart = actualStartDate.value_or(scheduledRange.getStartDate());
    if (returnDate < effectiveStart) {
        throw core::InvalidDateRangeException(
            "Actual return date (" + returnDate.toString() +
            ") cannot precede rental start date (" + effectiveStart.toString() + ").");
    }
    if (finalSettledCost < 0.0 || !std::isfinite(finalSettledCost)) {
        throw core::ValidationException(
            "Final rental cost cannot be negative, received: " + std::to_string(finalSettledCost));
    }

    actualReturnDate = returnDate;
    finalCost = finalSettledCost;
    status = core::RentalStatus::Completed;
}

RentalRecord RentalRecord::restoreFromStorage(
    int rId,
    int cId,
    int vId,
    core::DateRange range,
    std::optional<core::Date> actualStart,
    std::optional<core::Date> actualReturn,
    double estCost,
    std::optional<double> finCost,
    core::RentalStatus stat) {

    if (rId <= 0) {
        throw core::StorageException("Corrupted RentalRecord: rentalId must be positive, received: " + std::to_string(rId));
    }
    if (cId <= 0) {
        throw core::StorageException("Corrupted RentalRecord: customerId must be positive, received: " + std::to_string(cId));
    }
    if (vId <= 0) {
        throw core::StorageException("Corrupted RentalRecord: vehicleId must be positive, received: " + std::to_string(vId));
    }
    if (estCost < 0.0 || !std::isfinite(estCost)) {
        throw core::StorageException("Corrupted RentalRecord: estimatedCost must be non-negative and finite.");
    }

    // Validate state‑specific constraints
    if (stat == core::RentalStatus::Confirmed || stat == core::RentalStatus::Cancelled) {
        // No dates should be present for Confirmed or Cancelled rentals.
        if (actualStart.has_value() || actualReturn.has_value() || finCost.has_value()) {
            throw core::StorageException(
                "Corrupted RentalRecord #" + std::to_string(rId) +
                ": " + core::toString(stat) + " rental cannot have actualStartDate, actualReturnDate or finalCost.");
        }
    } else if (stat == core::RentalStatus::Active) {
        // Active rentals may have an actualStartDate (new records) or not (legacy).
        if (actualReturn.has_value() || finCost.has_value()) {
            throw core::StorageException(
                "Corrupted RentalRecord #" + std::to_string(rId) +
                ": Active rental cannot have actualReturnDate or finalCost.");
        }
        if (actualStart.has_value() && *actualStart < range.getStartDate()) {
            throw core::StorageException(
                "Corrupted RentalRecord #" + std::to_string(rId) +
                ": actualStartDate (" + actualStart->toString() +
                ") cannot precede scheduled start date (" + range.getStartDate().toString() + ").");
        }
    } else if (stat == core::RentalStatus::Completed) {
        // Completed rentals: actualReturnDate and finalCost are required; actualStartDate is optional (legacy compatibility).
        if (!actualReturn.has_value() || !finCost.has_value()) {
            throw core::StorageException(
                "Corrupted RentalRecord #" + std::to_string(rId) +
                ": Completed rental must contain actualReturnDate and finalCost.");
        }
        if (*finCost < 0.0 || !std::isfinite(*finCost)) {
            throw core::StorageException(
                "Corrupted RentalRecord #" + std::to_string(rId) +
                ": Final cost must be non-negative and finite.");
        }
        if (actualStart.has_value()) {
            if (*actualStart < range.getStartDate()) {
                throw core::StorageException(
                    "Corrupted RentalRecord #" + std::to_string(rId) +
                    ": actualStartDate (" + actualStart->toString() +
                    ") precedes scheduled start date (" + range.getStartDate().toString() + ").");
            }
            if (*actualReturn < *actualStart) {
                throw core::StorageException(
                    "Corrupted RentalRecord #" + std::to_string(rId) +
                    ": actualReturnDate (" + actualReturn->toString() +
                    ") precedes actualStartDate (" + actualStart->toString() + ").");
            }
        } else {
            // Legacy Completed record without actualStartDate: validate actualReturnDate against scheduledStart
            if (*actualReturn < range.getStartDate()) {
                throw core::StorageException(
                    "Corrupted RentalRecord #" + std::to_string(rId) +
                    ": actualReturnDate (" + actualReturn->toString() +
                    ") precedes scheduled start date (" + range.getStartDate().toString() + ").");
            }
        }
    }

    RentalRecord record(rId, cId, vId, range, estCost);
    record.status = stat;
    record.actualStartDate = actualStart;
    record.actualReturnDate = actualReturn;
    record.finalCost = finCost;
    return record;
}

bool RentalRecord::conflictsWith(const core::DateRange& requestedRange, const core::Date& /*today*/) const {
    switch (status) {
        case core::RentalStatus::Cancelled:
            // Cancelled reservations release all holds immediately
            return false;

        case core::RentalStatus::Confirmed:
            // Un-activated reservations hold their contracted window
            return scheduledRange.overlaps(requestedRange);

        case core::RentalStatus::Completed: {
            // Completed rentals reflect ground truth: [actualStartDate, actualReturnDate].
            // A delayed pickup releases the unused days before the vehicle was actually
            // collected; an early return releases dates after actualReturnDate.
            const core::Date& effectiveStart = actualStartDate.value_or(scheduledRange.getStartDate());
            core::DateRange actualOccupancy(effectiveStart, *actualReturnDate);
            return actualOccupancy.overlaps(requestedRange);
        }

        case core::RentalStatus::Active: {
            // An Active rental is out on the road without an actual return date.
            // It occupies the vehicle continuously from its actual pickup date until check-in.
            // A requested period entirely before the actual pickup date does not conflict.
            const core::Date& effectiveStart = actualStartDate.value_or(scheduledRange.getStartDate());
            if (requestedRange.getEndDate() < effectiveStart) {
                return false;
            }
            // Any requested period extending to or beyond the actual pickup date conflicts
            // as long as the vehicle remains unreturned.
            return true;
        }
    }
    return false;
}

void RentalRecord::displayDetails(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [RENTAL RECORD #" << rentalId << "]\n"
       << " Customer ID     : " << customerId << "\n"
       << " Vehicle ID      : " << vehicleId << "\n"
       << " Scheduled Dates : " << scheduledRange.toString()
       << " (" << scheduledRange.durationDays() << " days inclusive)\n"
       << " Actual Return   : " << (actualReturnDate ? actualReturnDate->toString() : "Not Returned (Active/Pending)") << "\n"
       << " Estimated Rent  : ₹" << std::fixed << std::setprecision(2) << estimatedCost << "\n";
    if (finalCost.has_value()) {
        os << " Final Settled   : ₹" << std::fixed << std::setprecision(2) << *finalCost << "\n";
    } else {
        os << " Final Settled   : Pending Final Inspection\n";
    }
    os << " Status          : " << core::toString(status) << "\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

