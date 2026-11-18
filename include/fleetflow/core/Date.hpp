#pragma once

#include <string>
#include <iosfwd>

namespace fleetflow::core {

/**
 * @brief Represents a specific calendar date (Year, Month, Day) in the Gregorian calendar.
 * Provides calendar validation, leap year checks, formatting, and exact day arithmetic.
 */
class Date {
private:
    int year{1970};
    int month{1};
    int day{1};

public:
    Date() = default;
    Date(int y, int m, int d);

    // Getters
    int getYear() const noexcept { return year; }
    int getMonth() const noexcept { return month; }
    int getDay() const noexcept { return day; }

    // Static calendar validators
    static bool isLeapYear(int y) noexcept;
    static int daysInMonth(int y, int m);
    static bool isValid(int y, int m, int d) noexcept;

    // Day arithmetic
    /**
     * @brief Computes calendar day difference between this date and another date (other - *this).
     * @return Positive if other > *this, 0 if equal, negative if other < *this.
     */
    int daysBetween(const Date& other) const noexcept;

    // Serialization & Parsing
    std::string toString() const;
    static Date fromString(const std::string& str);

    // Comparison Operators
    bool operator<(const Date& other) const noexcept;
    bool operator<=(const Date& other) const noexcept;
    bool operator>(const Date& other) const noexcept;
    bool operator>=(const Date& other) const noexcept;
    bool operator==(const Date& other) const noexcept;
    bool operator!=(const Date& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& os, const Date& date);
};

} // namespace fleetflow::core

