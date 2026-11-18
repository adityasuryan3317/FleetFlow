#include <gtest/gtest.h>
#include "fleetflow/services/Validation.hpp"
#include "fleetflow/core/Exceptions.hpp"

// Positive amount validation
TEST(ValidationPositiveAmount, AcceptsFinitePositive) {
    EXPECT_NO_THROW(fleetflow::services::validatePositiveAmount(10.5, "TestAmount"));
}

TEST(ValidationPositiveAmount, RejectsZero) {
    EXPECT_THROW(fleetflow::services::validatePositiveAmount(0.0, "ZeroAmount"), fleetflow::core::ValidationException);
}

TEST(ValidationPositiveAmount, RejectsNegative) {
    EXPECT_THROW(fleetflow::services::validatePositiveAmount(-5.0, "NegAmount"), fleetflow::core::ValidationException);
}

TEST(ValidationPositiveAmount, RejectsNaN) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fleetflow::services::validatePositiveAmount(nan, "NaNAmount"), fleetflow::core::ValidationException);
}

TEST(ValidationPositiveAmount, RejectsInfinity) {
    double inf = std::numeric_limits<double>::infinity();
    EXPECT_THROW(fleetflow::services::validatePositiveAmount(inf, "InfAmount"), fleetflow::core::ValidationException);
}

// Non‑negative amount validation
TEST(ValidationNonNegativeAmount, AcceptsZero) {
    EXPECT_NO_THROW(fleetflow::services::validateNonNegativeAmount(0.0, "ZeroAmount"));
}

TEST(ValidationNonNegativeAmount, AcceptsPositive) {
    EXPECT_NO_THROW(fleetflow::services::validateNonNegativeAmount(12.34, "PosAmount"));
}

TEST(ValidationNonNegativeAmount, RejectsNegative) {
    EXPECT_THROW(fleetflow::services::validateNonNegativeAmount(-0.1, "NegAmount"), fleetflow::core::ValidationException);
}

// Date order validation
TEST(ValidateDateOrder, AcceptsValidOrder) {
    fleetflow::core::Date d1(2023, 1, 1);
    fleetflow::core::Date d2(2023, 1, 10);
    EXPECT_NO_THROW(fleetflow::services::validateDateOrder(d1, d2, "ValidOrder"));
}

TEST(ValidateDateOrder, RejectsInvalidOrder) {
    fleetflow::core::Date d1(2023, 5, 20);
    fleetflow::core::Date d2(2023, 5, 10);
    EXPECT_THROW(fleetflow::services::validateDateOrder(d1, d2, "InvalidOrder"), fleetflow::core::ValidationException);
}

// Date in range validation
TEST(ValidateDateInRange, AcceptsDateWithinRange) {
    fleetflow::core::Date start(2023, 3, 1);
    fleetflow::core::Date end(2023, 3, 5);
    fleetflow::core::DateRange range(start, end);
    fleetflow::core::Date inside(2023, 3, 3);
    EXPECT_NO_THROW(fleetflow::services::validateDateInRange(range, inside, "WithinRange"));
}

TEST(ValidateDateInRange, RejectsDateOutsideRange) {
    fleetflow::core::Date start(2023, 3, 1);
    fleetflow::core::Date end(2023, 3, 5);
    fleetflow::core::DateRange range(start, end);
    fleetflow::core::Date outside(2023, 3, 10);
    EXPECT_THROW(fleetflow::services::validateDateInRange(range, outside, "OutsideRange"), fleetflow::core::ValidationException);
}
