#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/DateRange.hpp"
#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <iostream>
#include <string>
#include <cassert>

using namespace fleetflow::core;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (line " << __LINE__ << ")" << std::endl; \
            g_testsFailed++; \
        } else { \
            g_testsPassed++; \
        } \
    } while (0)

void testDateValidation() {
    std::cout << "[RUNNING] testDateValidation..." << std::endl;

    // Leap year 2024
    TEST_ASSERT(Date::isLeapYear(2024) == true, "2024 is a leap year");
    TEST_ASSERT(Date::isLeapYear(2000) == true, "2000 is a leap year");
    TEST_ASSERT(Date::isLeapYear(1900) == false, "1900 is NOT a leap year");
    TEST_ASSERT(Date::isLeapYear(2023) == false, "2023 is NOT a leap year");

    // Feb 29
    TEST_ASSERT(Date::isValid(2024, 2, 29) == true, "2024-02-29 is valid");
    TEST_ASSERT(Date::isValid(2023, 2, 29) == false, "2023-02-29 is invalid");

    // Invalid months and days
    TEST_ASSERT(Date::isValid(2026, 0, 10) == false, "Month 0 is invalid");
    TEST_ASSERT(Date::isValid(2026, 13, 10) == false, "Month 13 is invalid");
    TEST_ASSERT(Date::isValid(2026, 4, 31) == false, "April has only 30 days");
    TEST_ASSERT(Date::isValid(2026, 5, 31) == true, "May has 31 days");

    // Constructor exception on bad date
    bool threw = false;
    try {
        Date badDate(2023, 2, 29);
    } catch (const InvalidDateException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Constructing 2023-02-29 throws InvalidDateException");
}

void testDateFormattingAndParsing() {
    std::cout << "[RUNNING] testDateFormattingAndParsing..." << std::endl;

    Date d(2026, 11, 10);
    TEST_ASSERT(d.toString() == "2026-11-10", "Date toString format is YYYY-MM-DD");

    Date parsed = Date::fromString("2026-11-10");
    TEST_ASSERT(parsed.getYear() == 2026, "Parsed year is 2026");
    TEST_ASSERT(parsed.getMonth() == 11, "Parsed month is 11");
    TEST_ASSERT(parsed.getDay() == 10, "Parsed day is 10");
    TEST_ASSERT(parsed == d, "Parsed date equals constructed date");

    bool threw = false;
    try {
        Date::fromString("2026/11/10");
    } catch (const InvalidDateException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Slash format throws InvalidDateException");

    threw = false;
    try {
        Date::fromString("invalid-date");
    } catch (const InvalidDateException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Malformed string throws InvalidDateException");
}

void testDateArithmeticAndComparisons() {
    std::cout << "[RUNNING] testDateArithmeticAndComparisons..." << std::endl;

    Date d1(2026, 11, 10);
    Date d2(2026, 11, 15);
    Date d3(2026, 11, 10);

    TEST_ASSERT(d1 < d2, "d1 < d2");
    TEST_ASSERT(d1 <= d2, "d1 <= d2");
    TEST_ASSERT(d2 > d1, "d2 > d1");
    TEST_ASSERT(d1 == d3, "d1 == d3");

    // Days difference
    TEST_ASSERT(d1.daysBetween(d2) == 5, "Difference between Nov 10 and Nov 15 is 5 days");
    TEST_ASSERT(d2.daysBetween(d1) == -5, "Reverse difference is -5 days");
    TEST_ASSERT(d1.daysBetween(d1) == 0, "Self difference is 0 days");

    // Across month boundary
    Date oct31(2026, 10, 31);
    Date nov1(2026, 11, 1);
    TEST_ASSERT(oct31.daysBetween(nov1) == 1, "Oct 31 to Nov 1 is 1 day");

    // Across leap day
    Date feb28_2024(2024, 2, 28);
    Date mar1_2024(2024, 3, 1);
    TEST_ASSERT(feb28_2024.daysBetween(mar1_2024) == 2, "Feb 28 to Mar 1 in leap year is 2 days");
}

void testInclusiveRentalDuration() {
    std::cout << "[RUNNING] testInclusiveRentalDuration..." << std::endl;

    // Clarification requirement: Nov 10 to Nov 15 counts as 6 inclusive days
    Date start(2026, 11, 10);
    Date end(2026, 11, 15);
    DateRange range(start, end);

    TEST_ASSERT(range.durationDays() == 6, "Nov 10 to Nov 15 inclusive is 6 days");

    // Single-day rental
    DateRange singleDay(start, start);
    TEST_ASSERT(singleDay.durationDays() == 1, "Nov 10 to Nov 10 inclusive is 1 day");
}

void testDateRangeOverlapLogic() {
    std::cout << "[RUNNING] testDateRangeOverlapLogic..." << std::endl;

    DateRange base(Date(2026, 11, 10), Date(2026, 11, 15));

    // Case 1: Identical range
    DateRange identical(Date(2026, 11, 10), Date(2026, 11, 15));
    TEST_ASSERT(base.overlaps(identical) == true, "Identical ranges overlap");

    // Case 2: Boundary overlap (touching day 15).
    // Because dates are inclusive calendar days, Day 15 is occupied until end of day.
    DateRange touchesEnd(Date(2026, 11, 15), Date(2026, 11, 20));
    TEST_ASSERT(base.overlaps(touchesEnd) == true, "Boundary touch on day 15 overlaps");

    // Case 3: Strictly adjacent (Day 16 start)
    DateRange nextDay(Date(2026, 11, 16), Date(2026, 11, 20));
    TEST_ASSERT(base.overlaps(nextDay) == false, "Adjacent range (Day 16) does NOT overlap");

    // Case 4: Strictly prior adjacent (Day 9 end)
    DateRange priorDay(Date(2026, 11, 5), Date(2026, 11, 9));
    TEST_ASSERT(base.overlaps(priorDay) == false, "Prior range (Day 9 end) does NOT overlap");

    // Case 5: Sub-range / Enclosed
    DateRange enclosed(Date(2026, 11, 12), Date(2026, 11, 14));
    TEST_ASSERT(base.overlaps(enclosed) == true, "Enclosed sub-range overlaps");

    // Case 6: Partial overlap on start
    DateRange partialStart(Date(2026, 11, 8), Date(2026, 11, 11));
    TEST_ASSERT(base.overlaps(partialStart) == true, "Partial start overlap overlaps");

    // Contains check
    TEST_ASSERT(base.contains(Date(2026, 11, 10)) == true, "Range contains start date");
    TEST_ASSERT(base.contains(Date(2026, 11, 13)) == true, "Range contains middle date");
    TEST_ASSERT(base.contains(Date(2026, 11, 15)) == true, "Range contains end date");
    TEST_ASSERT(base.contains(Date(2026, 11, 16)) == false, "Range does not contain day 16");
}

void testInvalidDateRangeThrows() {
    std::cout << "[RUNNING] testInvalidDateRangeThrows..." << std::endl;

    bool threw = false;
    try {
        DateRange invalidRange(Date(2026, 11, 15), Date(2026, 11, 10));
    } catch (const InvalidDateRangeException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Constructing range with endDate < startDate throws InvalidDateRangeException");
}

void testCommonTypesConversions() {
    std::cout << "[RUNNING] testCommonTypesConversions..." << std::endl;

    TEST_ASSERT(toString(VehicleStatus::Available) == "Available", "VehicleStatus::Available toString");
    TEST_ASSERT(vehicleStatusFromString("UnderMaintenance") == VehicleStatus::UnderMaintenance, "vehicleStatusFromString");

    TEST_ASSERT(toString(RentalStatus::Active) == "Active", "RentalStatus::Active toString");
    TEST_ASSERT(rentalStatusFromString("Confirmed") == RentalStatus::Confirmed, "rentalStatusFromString");

    TEST_ASSERT(toString(PaymentType::Refund) == "Refund", "PaymentType::Refund toString");
    TEST_ASSERT(paymentTypeFromString("Payment") == PaymentType::Payment, "paymentTypeFromString");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  FleetFlow Test Suite: Date & Core     " << std::endl;
    std::cout << "========================================" << std::endl;

    testDateValidation();
    testDateFormattingAndParsing();
    testDateArithmeticAndComparisons();
    testInclusiveRentalDuration();
    testDateRangeOverlapLogic();
    testInvalidDateRangeThrows();
    testCommonTypesConversions();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (g_testsFailed == 0) ? 0 : 1;
}

