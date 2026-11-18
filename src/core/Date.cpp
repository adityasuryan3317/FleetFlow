#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <iomanip>
#include <sstream>
#include <tuple>

namespace fleetflow::core {

namespace {

/**
 * @brief Howard Hinnant's standard civil day count algorithm.
 * Converts Gregorian calendar (y, m, d) to linear days count relative to Unix epoch (1970-01-01).
 */
long long toCivilDays(int y, int m, int d) noexcept {
    y -= (m <= 2 ? 1 : 0);
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + static_cast<unsigned>(d) - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

} // anonymous namespace

Date::Date(int y, int m, int d) {
    if (!isValid(y, m, d)) {
        std::ostringstream oss;
        oss << "Invalid calendar date specified: " << y << "-" << m << "-" << d;
        throw InvalidDateException(oss.str());
    }
    year = y;
    month = m;
    day = d;
}

bool Date::isLeapYear(int y) noexcept {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int Date::daysInMonth(int y, int m) {
    if (m < 1 || m > 12) {
        throw InvalidDateException("Month out of range (1-12): " + std::to_string(m));
    }
    switch (m) {
        case 1:  return 31;
        case 2:  return isLeapYear(y) ? 29 : 28;
        case 3:  return 31;
        case 4:  return 30;
        case 5:  return 31;
        case 6:  return 30;
        case 7:  return 31;
        case 8:  return 31;
        case 9:  return 30;
        case 10: return 31;
        case 11: return 30;
        case 12: return 31;
        default: return 0;
    }
}

bool Date::isValid(int y, int m, int d) noexcept {
    if (y < 1 || m < 1 || m > 12) {
        return false;
    }
    int maxDays = 0;
    try {
        maxDays = daysInMonth(y, m);
    } catch (...) {
        return false;
    }
    return (d >= 1 && d <= maxDays);
}

int Date::daysBetween(const Date& other) const noexcept {
    long long thisDays = toCivilDays(year, month, day);
    long long otherDays = toCivilDays(other.year, other.month, other.day);
    return static_cast<int>(otherDays - thisDays);
}

std::string Date::toString() const {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << year << '-'
        << std::setw(2) << month << '-'
        << std::setw(2) << day;
    return oss.str();
}

Date Date::fromString(const std::string& str) {
    // Expected format: YYYY-MM-DD (length 10)
    if (str.length() != 10 || str[4] != '-' || str[7] != '-') {
        throw InvalidDateException("Expected date format YYYY-MM-DD, received: " + str);
    }

    try {
        int y = std::stoi(str.substr(0, 4));
        int m = std::stoi(str.substr(5, 2));
        int d = std::stoi(str.substr(8, 2));
        return Date(y, m, d);
    } catch (const InvalidDateException&) {
        throw;
    } catch (const std::exception& e) {
        throw InvalidDateException("Failed to parse date string '" + str + "': " + e.what());
    }
}

bool Date::operator<(const Date& other) const noexcept {
    return std::tie(year, month, day) < std::tie(other.year, other.month, other.day);
}

bool Date::operator<=(const Date& other) const noexcept {
    return std::tie(year, month, day) <= std::tie(other.year, other.month, other.day);
}

bool Date::operator>(const Date& other) const noexcept {
    return std::tie(year, month, day) > std::tie(other.year, other.month, other.day);
}

bool Date::operator>=(const Date& other) const noexcept {
    return std::tie(year, month, day) >= std::tie(other.year, other.month, other.day);
}

bool Date::operator==(const Date& other) const noexcept {
    return std::tie(year, month, day) == std::tie(other.year, other.month, other.day);
}

bool Date::operator!=(const Date& other) const noexcept {
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& os, const Date& date) {
    os << date.toString();
    return os;
}

} // namespace fleetflow::core

