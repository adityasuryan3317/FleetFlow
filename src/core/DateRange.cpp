#include "fleetflow/core/DateRange.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include <algorithm>

namespace fleetflow::core {

DateRange::DateRange(const Date& start, const Date& end) {
    if (end < start) {
        throw InvalidDateRangeException("Start date (" + start.toString() +
                                       ") cannot be after end date (" + end.toString() + ")");
    }
    startDate = start;
    endDate = end;
}

int DateRange::durationDays() const noexcept {
    // Inclusive calendar day duration: [Nov 10, Nov 15] = (15 - 10) + 1 = 6 days.
    return startDate.daysBetween(endDate) + 1;
}

bool DateRange::overlaps(const DateRange& other) const noexcept {
    // Closed intervals [S1, E1] and [S2, E2] overlap iff max(S1, S2) <= min(E1, E2)
    const Date& maxStart = (startDate > other.startDate) ? startDate : other.startDate;
    const Date& minEnd   = (endDate < other.endDate)     ? endDate   : other.endDate;
    return maxStart <= minEnd;
}

bool DateRange::contains(const Date& date) const noexcept {
    return (startDate <= date && date <= endDate);
}

std::string DateRange::toString() const {
    return "[" + startDate.toString() + " to " + endDate.toString() + "]";
}

bool DateRange::operator==(const DateRange& other) const noexcept {
    return (startDate == other.startDate && endDate == other.endDate);
}

bool DateRange::operator!=(const DateRange& other) const noexcept {
    return !(*this == other);
}

} // namespace fleetflow::core

