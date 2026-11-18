#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"
#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/DateRange.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <iostream>
#include <sstream>
#include <cmath>

using namespace fleetflow::models;
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

#define TEST_ASSERT_NEAR(val1, val2, eps, msg) \
    TEST_ASSERT(std::fabs((val1) - (val2)) < (eps), msg)

void testRentalRecordLifecycle() {
    std::cout << "[RUNNING] testRentalRecordLifecycle..." << std::endl;

    Date start(2026, 11, 10);
    Date expectedEnd(2026, 11, 15);
    DateRange range(start, expectedEnd);

    // 1. Initial construction must be Confirmed
    RentalRecord rental(1, 101, 201, range, 3000.0);
    TEST_ASSERT(rental.getRentalId() == 1, "Rental ID is 1");
    TEST_ASSERT(rental.getCustomerId() == 101, "Customer ID is 101");
    TEST_ASSERT(rental.getVehicleId() == 201, "Vehicle ID is 201");
    TEST_ASSERT(rental.getStatus() == RentalStatus::Confirmed, "Status starts as Confirmed");
    TEST_ASSERT(rental.getActualStartDate().has_value() == false, "Actual start date starts nullopt");
    TEST_ASSERT(rental.getActualReturnDate().has_value() == false, "Actual return date starts nullopt");
    TEST_ASSERT(rental.getFinalCost().has_value() == false, "Final cost starts nullopt");
    TEST_ASSERT(rental.getEstimatedCost() == 3000.0, "Estimated cost is 3000.0");

    // 2. Transition Confirmed -> Active (Check out)
    rental.markActive(start);
    TEST_ASSERT(rental.getStatus() == RentalStatus::Active, "Status transitioned to Active");
    TEST_ASSERT(rental.getActualStartDate().has_value() == true, "Actual start date is set");
    TEST_ASSERT(*rental.getActualStartDate() == start, "Actual start date matches Nov 10");

    // 3. Transition Active -> Completed (Check in)
    Date actualEnd(2026, 11, 15);
    rental.completeRental(actualEnd, 3000.0);
    TEST_ASSERT(rental.getStatus() == RentalStatus::Completed, "Status transitioned to Completed");
    TEST_ASSERT(rental.getActualReturnDate().has_value() == true, "Actual return date is set");
    TEST_ASSERT(*rental.getActualReturnDate() == actualEnd, "Actual return date matches Nov 15");
    TEST_ASSERT(rental.getFinalCost().has_value() == true, "Final cost is set");
    TEST_ASSERT(*rental.getFinalCost() == 3000.0, "Final cost matches 3000.0");
}

void testInvalidRentalTransitions() {
    std::cout << "[RUNNING] testInvalidRentalTransitions..." << std::endl;

    DateRange range(Date(2026, 11, 10), Date(2026, 11, 15));

    // A. Cannot complete a Confirmed rental directly
    RentalRecord r1(1, 101, 201, range, 3000.0);
    bool threw = false;
    try {
        r1.completeRental(Date(2026, 11, 15), 3000.0);
    } catch (const InvalidOperationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Completing Confirmed rental throws InvalidOperationException");

    // B. Cannot cancel an Active rental
    r1.markActive(range.getStartDate());
    threw = false;
    try {
        r1.markCancelled();
    } catch (const InvalidOperationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Cancelling Active rental throws InvalidOperationException");

    // C. Cannot activate an already Completed rental
    r1.completeRental(Date(2026, 11, 15), 3000.0);
    threw = false;
    try {
        r1.markActive(range.getStartDate());
    } catch (const InvalidOperationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Activating Completed rental throws InvalidOperationException");

    // D. Return date before start date throws InvalidDateRangeException
    RentalRecord r2(2, 101, 201, range, 3000.0);
    r2.markActive(range.getStartDate());
    threw = false;
    try {
        r2.completeRental(Date(2026, 11, 5), 1000.0);
    } catch (const InvalidDateRangeException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Completion date earlier than start date throws InvalidDateRangeException");

    // E. Negative settled cost throws ValidationException
    threw = false;
    try {
        r2.completeRental(Date(2026, 11, 15), -500.0);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Negative final settled cost throws ValidationException");
}

void testRentalConflictAndOccupancyRules() {
    std::cout << "[RUNNING] testRentalConflictAndOccupancyRules..." << std::endl;

    Date start(2026, 11, 10);
    Date end(2026, 11, 15);
    DateRange scheduled(start, end);
    Date todayOnSchedule(2026, 11, 12);
    Date todayOverdue(2026, 11, 17);

    // 1. CONFIRMED RENTAL
    RentalRecord confirmed(1, 101, 201, scheduled, 3000.0);
    // Overlapping query: Nov 12 to Nov 14
    TEST_ASSERT(confirmed.conflictsWith(DateRange(Date(2026, 11, 12), Date(2026, 11, 14)), todayOnSchedule) == true,
                "Confirmed rental conflicts with overlapping dates");
    // Non-overlapping future query: Nov 16 to Nov 20
    TEST_ASSERT(confirmed.conflictsWith(DateRange(Date(2026, 11, 16), Date(2026, 11, 20)), todayOnSchedule) == false,
                "Confirmed rental does NOT conflict with adjacent future dates (Nov 16-20)");

    // 2. CANCELLED RENTAL
    RentalRecord cancelled(2, 101, 201, scheduled, 3000.0);
    cancelled.markCancelled();
    TEST_ASSERT(cancelled.conflictsWith(DateRange(Date(2026, 11, 10), Date(2026, 11, 15)), todayOnSchedule) == false,
                "Cancelled rental occupies NO dates; returns false even for identical range");

    // 3. ACTIVE RENTAL OCCUPANCY (UNRETURNED VEHICLE)
    // Scheduled: Nov 10 to Nov 15. Active on road, actualReturnDate is nullopt.
    RentalRecord active(3, 101, 201, scheduled, 3000.0);
    active.markActive(scheduled.getStartDate());

    // Required Test Case 3: Request entirely before the existing rental start date (Nov 1 to Nov 5)
    TEST_ASSERT(active.conflictsWith(DateRange(Date(2026, 11, 1), Date(2026, 11, 5)), todayOnSchedule) == false,
                "Active rental does NOT conflict with requested period entirely before start date (Nov 1-5)");

    // Overlapping during scheduled interval (Nov 12 to Nov 14)
    TEST_ASSERT(active.conflictsWith(DateRange(Date(2026, 11, 12), Date(2026, 11, 14)), todayOnSchedule) == true,
                "Active rental conflicts with overlapping dates in scheduled window");

    // Required Test Case 1: Future booking immediately after scheduled return date (Nov 16 to Nov 18)
    // Even though today is Nov 12 (before scheduled end), the unreturned active rental locks the vehicle.
    TEST_ASSERT(active.conflictsWith(DateRange(Date(2026, 11, 16), Date(2026, 11, 18)), todayOnSchedule) == true,
                "Active rental conflicts with future booking immediately after scheduled return date (Nov 16-18)");

    // Required Test Case 2: Future booking several weeks later (Dec 1 to Dec 5)
    TEST_ASSERT(active.conflictsWith(DateRange(Date(2026, 12, 1), Date(2026, 12, 5)), todayOnSchedule) == true,
                "Active rental conflicts with future booking several weeks later (Dec 1-5)");

    // Overdue active rental (today is Nov 17, still unreturned): locks future booking (Nov 20 to Nov 25)
    TEST_ASSERT(active.conflictsWith(DateRange(Date(2026, 11, 20), Date(2026, 11, 25)), todayOverdue) == true,
                "Overdue active rental continues locking all future dates");

    // Required Test Case 4: COMPLETED RENTAL: EARLY RETURN RELEASES FUTURE DATES
    RentalRecord earlyReturn(4, 101, 201, scheduled, 3000.0);
    earlyReturn.markActive(scheduled.getStartDate());
    // Returned early on Nov 12 (expected was Nov 15)
    earlyReturn.completeRental(Date(2026, 11, 12), 1500.0);
    // Nov 10-12 is occupied
    TEST_ASSERT(earlyReturn.conflictsWith(DateRange(Date(2026, 11, 11), Date(2026, 11, 12)), todayOnSchedule) == true,
                "Completed early return occupies actual period (Nov 10-12)");
    // Nov 13-15 was scheduled, but customer returned early: these dates MUST be released!
    TEST_ASSERT(earlyReturn.conflictsWith(DateRange(Date(2026, 11, 13), Date(2026, 11, 15)), todayOnSchedule) == false,
                "Completed early return releases unused scheduled dates (Nov 13-15)");
    // Future dates (Nov 16-18) are also completely free
    TEST_ASSERT(earlyReturn.conflictsWith(DateRange(Date(2026, 11, 16), Date(2026, 11, 18)), todayOnSchedule) == false,
                "Completed early return does not conflict with post-scheduled dates (Nov 16-18)");

    // Required Test Case 5: COMPLETED RENTAL: LATE RETURN EXTENDS OCCUPIED DATES
    RentalRecord lateReturn(5, 101, 201, scheduled, 3000.0);
    lateReturn.markActive(scheduled.getStartDate());
    // Returned late on Nov 17 (expected was Nov 15)
    lateReturn.completeRental(Date(2026, 11, 17), 4200.0);
    // Nov 16 to Nov 18: overlaps with the extended actual occupancy (through Nov 17)
    TEST_ASSERT(lateReturn.conflictsWith(DateRange(Date(2026, 11, 16), Date(2026, 11, 18)), todayOverdue) == true,
                "Completed late return extends occupancy through actual return date (Nov 17)");
    // Nov 18 to Nov 20: after actual return date, free!
    TEST_ASSERT(lateReturn.conflictsWith(DateRange(Date(2026, 11, 18), Date(2026, 11, 20)), todayOverdue) == false,
                "Completed late return frees dates after actual return date (Nov 18 onward)");


    // 6. DELAYED PICKUP: OCCUPANCY BEGINS ON THE ACTUAL PICKUP DATE
    RentalRecord delayedPickup(6, 101, 201, scheduled, 3000.0);
    delayedPickup.markActive(Date(2026, 11, 12));
    // The reservation was scheduled from Nov 10, but the vehicle was not collected until Nov 12.
    TEST_ASSERT(delayedPickup.conflictsWith(DateRange(Date(2026, 11, 10), Date(2026, 11, 11)), todayOnSchedule) == false,
                "Active delayed-pickup rental releases dates before actual pickup (Nov 10-11)");
    TEST_ASSERT(delayedPickup.conflictsWith(DateRange(Date(2026, 11, 11), Date(2026, 11, 12)), todayOnSchedule) == true,
                "Active delayed-pickup rental conflicts from actual pickup date (Nov 12)");
    TEST_ASSERT(delayedPickup.conflictsWith(DateRange(Date(2026, 11, 13), Date(2026, 11, 14)), todayOnSchedule) == true,
                "Active delayed-pickup rental blocks future dates until check-in");

    delayedPickup.completeRental(Date(2026, 11, 14), 1800.0);
    TEST_ASSERT(delayedPickup.conflictsWith(DateRange(Date(2026, 11, 10), Date(2026, 11, 11)), todayOnSchedule) == false,
                "Completed delayed-pickup rental does not occupy dates before actual pickup");
    TEST_ASSERT(delayedPickup.conflictsWith(DateRange(Date(2026, 11, 12), Date(2026, 11, 14)), todayOnSchedule) == true,
                "Completed delayed-pickup rental occupies actual pickup through actual return");
    TEST_ASSERT(delayedPickup.conflictsWith(DateRange(Date(2026, 11, 15), Date(2026, 11, 16)), todayOnSchedule) == false,
                "Completed delayed-pickup rental releases dates after actual return");
}

void testPaymentRecordLedger() {
    std::cout << "[RUNNING] testPaymentRecordLedger..." << std::endl;

    Date today(2026, 11, 10);

    // Inflow payment
    PaymentRecord pay(1, 101, 201, 3500.0, today, PaymentType::Payment, PaymentMethod::UPI, "Advance rental booking deposit");
    TEST_ASSERT(pay.getPaymentId() == 1, "Payment ID is 1");
    TEST_ASSERT(pay.getRentalId() == 101, "Rental ID is 101");
    TEST_ASSERT(pay.getCustomerId() == 201, "Customer ID is 201");
    TEST_ASSERT(pay.getAmount() == 3500.0, "Amount is 3500.0");
    TEST_ASSERT(pay.getPaymentType() == PaymentType::Payment, "Type is Payment");
    TEST_ASSERT(pay.getPaymentMethod() == PaymentMethod::UPI, "Method is UPI");
    TEST_ASSERT(pay.getDescription() == "Advance rental booking deposit", "Description matches");

    // Outflow refund
    PaymentRecord refund(2, 101, 201, 1200.0, today, PaymentType::Refund, PaymentMethod::Card, "Early check-in unused balance refund");
    TEST_ASSERT(refund.getPaymentType() == PaymentType::Refund, "Type is Refund");
    TEST_ASSERT(refund.getAmount() == 1200.0, "Refund amount is 1200.0");

    // Validation rejection on amount <= 0
    bool threw = false;
    try {
        PaymentRecord badAmt(3, 101, 201, 0.0, today, PaymentType::Payment, PaymentMethod::Cash, "Zero amount");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Zero amount throws ValidationException");

    threw = false;
    try {
        PaymentRecord negativeAmt(4, 101, 201, -150.0, today, PaymentType::Payment, PaymentMethod::Cash, "Negative amount");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Negative amount throws ValidationException");

    // Stream formatting with INR
    std::ostringstream oss;
    pay.displayDetails(oss);
    std::string out = oss.str();
    TEST_ASSERT(out.find("Amount          : ₹3500.00") != std::string::npos, "Formats amount with INR ₹ and 2 decimals");
}

void testMaintenanceRecordLifecycle() {
    std::cout << "[RUNNING] testMaintenanceRecordLifecycle..." << std::endl;

    Date start(2026, 11, 1);

    // Initial state must be Scheduled
    MaintenanceRecord record(1, 301, "Brake pads replacement and wheel alignment", start, 2500.0);
    TEST_ASSERT(record.getMaintenanceId() == 1, "Maintenance ID is 1");
    TEST_ASSERT(record.getVehicleId() == 301, "Vehicle ID is 301");
    TEST_ASSERT(record.getStatus() == MaintenanceStatus::Scheduled, "Status starts as Scheduled");
    TEST_ASSERT(record.getCompletionDate().has_value() == false, "Completion date is nullopt");
    TEST_ASSERT(record.getCost() == 2500.0, "Initial estimated cost is 2500.0");

    // Transition Scheduled -> InProgress
    record.startMaintenance();
    TEST_ASSERT(record.getStatus() == MaintenanceStatus::InProgress, "Status transitioned to InProgress");

    // Cost adjustment while in progress
    record.updateCost(2800.0);
    TEST_ASSERT(record.getCost() == 2800.0, "Cost updated to 2800.0");

    // Transition InProgress -> Completed
    Date finish(2026, 11, 3);
    record.completeMaintenance(finish, 2800.0);
    TEST_ASSERT(record.getStatus() == MaintenanceStatus::Completed, "Status transitioned to Completed");
    TEST_ASSERT(record.getCompletionDate().has_value() == true, "Completion date is recorded");
    TEST_ASSERT(*record.getCompletionDate() == finish, "Completion date matches Nov 3");

    // Invalid transitions
    bool threw = false;
    try {
        record.startMaintenance();
    } catch (const InvalidOperationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Starting already completed maintenance throws InvalidOperationException");

    // Negative cost throws ValidationException
    threw = false;
    try {
        record.updateCost(-100.0);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Negative cost update throws ValidationException");

    // Completion date before start date throws InvalidDateRangeException
    MaintenanceRecord r2(2, 302, "Oil filter change", Date(2026, 11, 5), 1000.0);
    r2.startMaintenance();
    threw = false;
    try {
        r2.completeMaintenance(Date(2026, 11, 2), 1000.0);
    } catch (const InvalidDateRangeException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Completion date earlier than start date throws InvalidDateRangeException");
}

void testActualStartDateRules() {
    std::cout << "[RUNNING] testActualStartDateRules..." << std::endl;

    DateRange range(Date(2026, 11, 10), Date(2026, 11, 15));

    // 1. Actual start-date recording
    RentalRecord r1(1, 101, 201, range, 3000.0);
    TEST_ASSERT(!r1.getActualStartDate().has_value(), "Newly created rental has nullopt actualStartDate");

    // markActive with date before scheduled start throws InvalidDateRangeException
    bool threw = false;
    try {
        r1.markActive(Date(2026, 11, 9));
    } catch (const InvalidDateRangeException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "markActive with date before scheduled start throws InvalidDateRangeException");

    // markActive with valid date (Nov 12, delayed pickup) succeeds
    r1.markActive(Date(2026, 11, 12));
    TEST_ASSERT(r1.getStatus() == RentalStatus::Active, "Rental is now Active");
    TEST_ASSERT(r1.getActualStartDate().has_value(), "actualStartDate is recorded");
    TEST_ASSERT(*r1.getActualStartDate() == Date(2026, 11, 12), "actualStartDate matches Nov 12");

    // 2. Actual return before actual start rejected
    // Scheduled start is Nov 10, actual start is Nov 12. Return attempt on Nov 11 must be rejected!
    threw = false;
    try {
        r1.completeRental(Date(2026, 11, 11), 3000.0);
    } catch (const InvalidDateRangeException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Return date between scheduled start and actual start throws InvalidDateRangeException");

    // Valid return on or after actual start succeeds
    threw = false;
    try {
        r1.completeRental(Date(2026, 11, 14), 3000.0);
    } catch (const std::exception&) {
        threw = true;
    }
    TEST_ASSERT(!threw, "Return date >= actualStartDate completes successfully");
    TEST_ASSERT(r1.getStatus() == RentalStatus::Completed, "Rental is completed");
    TEST_ASSERT(r1.getActualReturnDate().has_value() && *r1.getActualReturnDate() == Date(2026, 11, 14), "Return date matches Nov 14");

    // 3. Legacy Active rental without actualStartDate falls back to scheduled start
    RentalRecord legacyActive = RentalRecord::restoreFromStorage(
        2, 101, 201, range, std::nullopt, std::nullopt, 3000.0, std::nullopt, RentalStatus::Active);
    TEST_ASSERT(!legacyActive.getActualStartDate().has_value(), "Legacy active rental has no actualStartDate");

    // Return date before scheduled start throws
    threw = false;
    try {
        legacyActive.completeRental(Date(2026, 11, 9), 3000.0);
    } catch (const InvalidDateRangeException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Legacy active rental rejects returnDate before scheduled start");

    // Return date on or after scheduled start succeeds
    threw = false;
    try {
        legacyActive.completeRental(Date(2026, 11, 10), 3000.0);
    } catch (const std::exception&) {
        threw = true;
    }
    TEST_ASSERT(!threw, "Legacy active rental accepts returnDate >= scheduled start");
    TEST_ASSERT(legacyActive.getStatus() == RentalStatus::Completed, "Legacy rental completed");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " FleetFlow Test Suite: Operational Records" << std::endl;
    std::cout << "========================================" << std::endl;

    testRentalRecordLifecycle();
    testInvalidRentalTransitions();
    testActualStartDateRules();
    testRentalConflictAndOccupancyRules();
    testPaymentRecordLedger();
    testMaintenanceRecordLifecycle();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (g_testsFailed == 0) ? 0 : 1;
}

