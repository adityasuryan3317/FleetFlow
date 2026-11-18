#include "fleetflow/services/Validation.hpp"

#include <limits>
#include <cmath>

#include "fleetflow/core/Exceptions.hpp"

namespace fleetflow::services {

void validatePositiveAmount(double amount, const std::string& name) {
    if (!std::isfinite(amount) || amount <= 0.0) {
        throw fleetflow::core::ValidationException(
            name + " must be a finite positive value (got " + std::to_string(amount) + ")");
    }
}

void validateNonNegativeAmount(double amount, const std::string& name) {
    if (!std::isfinite(amount) || amount < 0.0) {
        throw fleetflow::core::ValidationException(
            name + " must be a finite non‑negative value (got " + std::to_string(amount) + ")");
    }
}

void validateDateOrder(const fleetflow::core::Date& start,
                       const fleetflow::core::Date& end,
                       const std::string& context) {
    if (end < start) {
        throw fleetflow::core::ValidationException(
            context + ": end date " + end.toString() + " precedes start date " + start.toString());
    }
}

void validateDateInRange(const fleetflow::core::DateRange& range,
                         const fleetflow::core::Date& date,
                         const std::string& context) {
    if (!range.contains(date)) {
        throw fleetflow::core::ValidationException(
            context + ": date " + date.toString() + " is not within range " + range.toString());
    }
}

} // namespace fleetflow::services

