#pragma once

#include "fleetflow/core/Date.hpp"
#include <string>

namespace fleetflow::core {

/**
 * @brief Represents a closed, inclusive calendar interval [startDate, endDate].
 * Used for rental scheduling, availability checking, and billing duration calculations.
 */
class DateRange {
private:
    Date startDate;
    Date endDate;

public:
    DateRange() = default;

    /**
     * @brief Constructs a validated date range.
     * @param start The beginning of the interval (inclusive).
     * @param end The end of the interval (inclusive).
     * @throws InvalidDateRangeException if end < start.
     */
    DateRange(const Date& start, const Date& end);

    // Getters
    const Date& getStartDate() const noexcept { return startDate; }
    const Date& getEndDate() const noexcept { return endDate; }

    /**
     * @brief Computes the inclusive number of calendar days in this range.
     * E.g. Nov 10 to Nov 15 inclusive = 6 days.
     * E.g. Nov 10 to Nov 10 inclusive = 1 day.
     */
    int durationDays() const noexcept;

    /**
     * @brief Determines whether this range overlaps with another closed range.
     * Two closed intervals [S1, E1] and [S2, E2] overlap iff max(S1, S2) <= min(E1, E2).
     */
    bool overlaps(const DateRange& other) const noexcept;

    /**
     * @brief Checks if a specific date falls within [startDate, endDate] inclusive.
     */
    bool contains(const Date& date) const noexcept;

    std::string toString() const;

    bool operator==(const DateRange& other) const noexcept;
    bool operator!=(const DateRange& other) const noexcept;
};

} // namespace fleetflow::core

