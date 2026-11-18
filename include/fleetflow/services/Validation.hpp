#pragma once

#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/DateRange.hpp"

namespace fleetflow::services {

/**
 * @brief Validates that a monetary amount is finite and strictly positive.
 *
 * @param amount The amount to validate.
 * @param name   Optional identifier used in the error message (e.g. "Payment amount").
 * @throws fleetflow::core::ValidationException if the amount is NaN, infinite, or <= 0.
 */
void validatePositiveAmount(double amount, const std::string& name = "Amount");

/**
 * @brief Validates that a monetary amount is finite and non‑negative (zero allowed).
 *
 * @param amount The amount to validate.
 * @param name   Optional identifier for error messages.
 * @throws fleetflow::core::ValidationException if the amount is NaN, infinite, or < 0.
 */
void validateNonNegativeAmount(double amount, const std::string& name = "Amount");

/**
 * @brief Validates that `start` does not come after `end`.
 *
 * @param start   The earlier date.
 * @param end     The later date.
 * @param context Optional context string used in the exception message.
 * @throws fleetflow::core::ValidationException if `start` > `end`.
 */
void validateDateOrder(const fleetflow::core::Date& start,
                       const fleetflow::core::Date& end,
                       const std::string& context = "Date order");

/**
 * @brief Validates that a given `date` lies within a supplied `range` (inclusive).
 *
 * @param range   The DateRange to check against.
 * @param date    The Date that must be inside the range.
 * @param context Optional context for the error message.
 * @throws fleetflow::core::ValidationException if `date` is outside `range`.
 */
void validateDateInRange(const fleetflow::core::DateRange& range,
                         const fleetflow::core::Date& date,
                         const std::string& context = "Date in range");

} // namespace fleetflow::services

