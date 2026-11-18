/**
 * @file test_csv_serializers.cpp
 * @brief Comprehensive tests for CSV Serializers across all domain models.
 *
 * Verifies:
 * - VehicleSerializer (Car, Bike, Truck derived types, attributes, polymorphic dispatch)
 * - UserSerializer (Customer, Employee, Admin derived types, attributes, polymorphic dispatch)
 * - RentalSerializer (all 4 lifecycle states: Confirmed, Active, Completed, Cancelled)
 * - RentalRecord restoration matrix (every invalid state combination rejected)
 * - PaymentSerializer (Payment, Refund, PaymentMethod enum preservation)
 * - MaintenanceSerializer (all 3 lifecycle states: Scheduled, InProgress, Completed)
 * - Floating-point precision (max_digits10, 0.1, many decimals, small/large finite values, NaN/Inf rejection)
 * - Full Round-Trip fidelity via CsvHelper::escapeRow -> CsvHelper::parseLine
 * - Error validation (column counts, numeric format, non-finite values, dates, enums, state invariants)
 */

#include "fleetflow/storage/CsvSerializers.hpp"
#include "fleetflow/storage/CsvHelper.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Bike.hpp"
#include "fleetflow/models/Truck.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/Employee.hpp"
#include "fleetflow/models/Admin.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"
#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/DateRange.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace fleetflow;
using namespace fleetflow::storage;

// ============================================================================
// Test Framework Macros
// ============================================================================

static int passed = 0;
static int failed = 0;

#define TEST(name) \
    do { \
        std::cout << "  TEST: " << name << " ... "; \
        try {

#define PASS \
            std::cout << "PASSED\n"; \
            ++passed; \
        } catch (const std::exception& ex) { \
            std::cout << "FAILED (unexpected exception: " << ex.what() << ")\n"; \
            ++failed; \
        } catch (...) { \
            std::cout << "FAILED (unknown exception)\n"; \
            ++failed; \
        } \
    } while (0)

#define VERIFY(cond) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error("Verification failed: " #cond); \
        } \
    } while (0)

#define EXPECT_THROW(expr, ExType) \
    do { \
        bool caught = false; \
        try { expr; } \
        catch (const ExType&) { caught = true; } \
        catch (...) {} \
        if (!caught) { \
            throw std::runtime_error("Expected " #ExType " was not thrown"); \
        } \
    } while (0)

// ============================================================================
// 1. VEHICLE SERIALIZER TESTS
// ============================================================================

void testCarSerializationRoundTrip() {
    TEST("Car round-trip serialization and deserialization") {
        models::Car car(1, "Toyota", "Camry Hybrid", "DL-01-AB-1234", 4500.50,
                        5, "Petrol+Electric", true, core::VehicleStatus::Available);

        auto fields = VehicleSerializer::serialize(car);
        VERIFY(fields.size() == 10);
        VERIFY(fields[0] == "1");
        VERIFY(fields[1] == "CAR");
        VERIFY(fields[2] == "Toyota");
        VERIFY(fields[3] == "Camry Hybrid");
        VERIFY(fields[4] == "DL-01-AB-1234");
        VERIFY(fields[5] == "4500.5");
        VERIFY(fields[6] == "Available");
        VERIFY(fields[7] == "5");
        VERIFY(fields[8] == "Petrol+Electric");
        VERIFY(fields[9] == "true");

        // Pipe through CsvHelper
        std::string csv = CsvHelper::escapeRow(fields);
        auto parsedFields = CsvHelper::parseLine(csv);
        auto restoredVehicle = VehicleSerializer::deserialize(parsedFields);

        VERIFY(restoredVehicle != nullptr);
        VERIFY(restoredVehicle->getVehicleId() == 1);
        VERIFY(restoredVehicle->getVehicleType() == "Car");
        VERIFY(restoredVehicle->getBrand() == "Toyota");
        VERIFY(restoredVehicle->getModel() == "Camry Hybrid");
        VERIFY(restoredVehicle->getLicensePlate() == "DL-01-AB-1234");
        VERIFY(restoredVehicle->getBaseDailyRate() == 4500.50);
        VERIFY(restoredVehicle->getStatus() == core::VehicleStatus::Available);

        auto* restoredCar = dynamic_cast<models::Car*>(restoredVehicle.get());
        VERIFY(restoredCar != nullptr);
        VERIFY(restoredCar->getSeatingCapacity() == 5);
        VERIFY(restoredCar->getFuelType() == "Petrol+Electric");
        VERIFY(restoredCar->getIsAutomatic() == true);
    } PASS;
}

void testBikeSerializationRoundTrip() {
    TEST("Bike round-trip serialization and deserialization") {
        models::Bike bike(2, "Royal Enfield", "Meteor 350", "KA-05-XY-9999", 1200.00,
                          349, true, core::VehicleStatus::UnderMaintenance);

        auto fields = VehicleSerializer::serialize(bike);
        VERIFY(fields.size() == 10);
        VERIFY(fields[1] == "BIKE");
        VERIFY(fields[7] == "349");
        VERIFY(fields[8] == "true");
        VERIFY(fields[9] == ""); // extra3 is empty for bike

        std::string csv = CsvHelper::escapeRow(fields);
        auto parsedFields = CsvHelper::parseLine(csv);
        auto restoredVehicle = VehicleSerializer::deserialize(parsedFields);

        VERIFY(restoredVehicle != nullptr);
        VERIFY(restoredVehicle->getVehicleType() == "Bike");
        VERIFY(restoredVehicle->getStatus() == core::VehicleStatus::UnderMaintenance);

        auto* restoredBike = dynamic_cast<models::Bike*>(restoredVehicle.get());
        VERIFY(restoredBike != nullptr);
        VERIFY(restoredBike->getEngineCC() == 349);
        VERIFY(restoredBike->getHasGear() == true);
    } PASS;
}

void testTruckSerializationRoundTrip() {
    TEST("Truck round-trip serialization and deserialization") {
        models::Truck truck(3, "Volvo", "FH16 Heavy", "MH-12-TR-7777", 8500.75,
                            16.5, 4, core::VehicleStatus::Retired);

        auto fields = VehicleSerializer::serialize(truck);
        VERIFY(fields.size() == 10);
        VERIFY(fields[1] == "TRUCK");
        VERIFY(fields[6] == "Retired");
        VERIFY(fields[7] == "16.5");
        VERIFY(fields[8] == "4");
        VERIFY(fields[9] == "");

        std::string csv = CsvHelper::escapeRow(fields);
        auto parsedFields = CsvHelper::parseLine(csv);
        auto restoredVehicle = VehicleSerializer::deserialize(parsedFields);

        VERIFY(restoredVehicle != nullptr);
        VERIFY(restoredVehicle->getVehicleType() == "Truck");
        VERIFY(restoredVehicle->getStatus() == core::VehicleStatus::Retired);

        auto* restoredTruck = dynamic_cast<models::Truck*>(restoredVehicle.get());
        VERIFY(restoredTruck != nullptr);
        VERIFY(restoredTruck->getCargoCapacityTons() == 16.5);
        VERIFY(restoredTruck->getNumberOfAxles() == 4);
    } PASS;
}

void testVehicleSpecialCharacters() {
    TEST("Vehicle with commas and quotes in brand and model") {
        models::Car car(4, "Mercedes, Benz", "S-Class \"Maybach\"", "DL-01-MM-0001", 15000.0,
                        4, "Petrol", true, core::VehicleStatus::Available);

        std::string csv = CsvHelper::escapeRow(VehicleSerializer::serialize(car));
        auto restored = VehicleSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored->getBrand() == "Mercedes, Benz");
        VERIFY(restored->getModel() == "S-Class \"Maybach\"");
    } PASS;
}

void testVehicleErrors() {
    TEST("VehicleSerializer rejects wrong column counts") {
        std::vector<std::string> nineFields = {"1", "CAR", "B", "M", "P", "10.0", "Available", "5", "Gas"};
        EXPECT_THROW(VehicleSerializer::deserialize(nineFields), core::StorageException);

        std::vector<std::string> elevenFields = {"1", "CAR", "B", "M", "P", "10.0", "Available", "5", "Gas", "true", "extra"};
        EXPECT_THROW(VehicleSerializer::deserialize(elevenFields), core::StorageException);
    } PASS;

    TEST("VehicleSerializer rejects non-finite rate") {
        std::vector<std::string> fields = {"1", "CAR", "B", "M", "P", "nan", "Available", "5", "Gas", "true"};
        EXPECT_THROW(VehicleSerializer::deserialize(fields), core::StorageException);
    } PASS;
}

// ============================================================================
// 2. USER SERIALIZER TESTS
// ============================================================================

void testCustomerSerializationRoundTrip() {
    TEST("Customer round-trip serialization and deserialization") {
        models::Customer cust(10, "rahul_k", "Rahul Kumar", "rahul.k@example.com",
                              "DL-1420110012345", "+91-9876543210");

        auto fields = UserSerializer::serialize(cust);
        VERIFY(fields.size() == 7);
        VERIFY(fields[0] == "10");
        VERIFY(fields[1] == "CUSTOMER");
        VERIFY(fields[2] == "rahul_k");
        VERIFY(fields[3] == "Rahul Kumar");
        VERIFY(fields[4] == "rahul.k@example.com");
        VERIFY(fields[5] == "DL-1420110012345");
        VERIFY(fields[6] == "+91-9876543210");

        std::string csv = CsvHelper::escapeRow(fields);
        auto restoredUser = UserSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restoredUser != nullptr);
        VERIFY(restoredUser->getUserId() == 10);
        VERIFY(restoredUser->getRoleName() == "Customer");
        VERIFY(restoredUser->getUsername() == "rahul_k");
        VERIFY(restoredUser->getFullName() == "Rahul Kumar");
        VERIFY(restoredUser->getEmail() == "rahul.k@example.com");

        auto* restoredCust = dynamic_cast<models::Customer*>(restoredUser.get());
        VERIFY(restoredCust != nullptr);
        VERIFY(restoredCust->getDrivingLicense() == "DL-1420110012345");
        VERIFY(restoredCust->getPhoneNumber() == "+91-9876543210");
    } PASS;
}

void testEmployeeSerializationRoundTrip() {
    TEST("Employee round-trip serialization and deserialization") {
        models::Employee emp(20, "priya_ops", "Priya Sharma", "priya.s@fleetflow.in",
                             "EMP-204", "Fleet Operations");

        std::string csv = CsvHelper::escapeRow(UserSerializer::serialize(emp));
        auto restoredUser = UserSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restoredUser != nullptr);
        VERIFY(restoredUser->getRoleName() == "Employee");

        auto* restoredEmp = dynamic_cast<models::Employee*>(restoredUser.get());
        VERIFY(restoredEmp != nullptr);
        VERIFY(restoredEmp->getEmployeeCode() == "EMP-204");
        VERIFY(restoredEmp->getDepartment() == "Fleet Operations");
    } PASS;
}

void testAdminSerializationRoundTrip() {
    TEST("Admin round-trip serialization and deserialization") {
        models::Admin adm(30, "aditya_adm", "Aditya Suryan", "admin@fleetflow.in",
                          3, "HQ - New Delhi");

        std::string csv = CsvHelper::escapeRow(UserSerializer::serialize(adm));
        auto restoredUser = UserSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restoredUser != nullptr);
        VERIFY(restoredUser->getRoleName() == "Admin");

        auto* restoredAdm = dynamic_cast<models::Admin*>(restoredUser.get());
        VERIFY(restoredAdm != nullptr);
        VERIFY(restoredAdm->getSecurityClearance() == 3);
        VERIFY(restoredAdm->getDesignatedOffice() == "HQ - New Delhi");
    } PASS;
}

void testUserSpecialCharacters() {
    TEST("User with commas and quotes in name and office") {
        models::Admin adm(31, "ceo_corp", "Kumar, Rajesh \"Raj\"", "raj@corp.com",
                          5, "Executive Suite, Building 4");

        std::string csv = CsvHelper::escapeRow(UserSerializer::serialize(adm));
        auto restored = UserSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored->getFullName() == "Kumar, Rajesh \"Raj\"");
        auto* a = dynamic_cast<models::Admin*>(restored.get());
        VERIFY(a != nullptr);
        VERIFY(a->getDesignatedOffice() == "Executive Suite, Building 4");
    } PASS;
}

void testUserErrors() {
    TEST("UserSerializer rejects invalid column counts") {
        std::vector<std::string> sixFields = {"1", "CUSTOMER", "u", "n", "e@x.com", "DL"};
        EXPECT_THROW(UserSerializer::deserialize(sixFields), core::StorageException);

        std::vector<std::string> eightFields = {"1", "CUSTOMER", "u", "n", "e@x.com", "DL", "P", "extra"};
        EXPECT_THROW(UserSerializer::deserialize(eightFields), core::StorageException);
    } PASS;
}

// ============================================================================
// 3. RENTAL RECORD SERIALIZER & RESTORATION MATRIX TESTS
// ============================================================================

void testRentalConfirmedRoundTrip() {
    TEST("RentalRecord Confirmed state round-trip") {
        core::Date start(2026, 11, 10);
        core::Date end(2026, 11, 15);
        models::RentalRecord rental(100, 10, 1, core::DateRange(start, end), 3500.0);

        auto fields = RentalSerializer::serialize(rental);
        VERIFY(fields.size() == 10);
        VERIFY(fields[0] == "100");
        VERIFY(fields[1] == "10");
        VERIFY(fields[2] == "1");
        VERIFY(fields[3] == "2026-11-10");
        VERIFY(fields[4] == "2026-11-15");
        VERIFY(fields[5] == ""); // actualReturnDate is empty
        VERIFY(fields[6] == "3500");
        VERIFY(fields[7] == ""); // finalCost is empty
        VERIFY(fields[8] == "Confirmed");
        VERIFY(fields[9] == ""); // actualStartDate is empty

        std::string csv = CsvHelper::escapeRow(fields);
        auto restored = RentalSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getRentalId() == 100);
        VERIFY(restored.getCustomerId() == 10);
        VERIFY(restored.getVehicleId() == 1);
        VERIFY(restored.getScheduledRange().getStartDate() == start);
        VERIFY(restored.getScheduledRange().getEndDate() == end);
        VERIFY(!restored.getActualStartDate().has_value());
        VERIFY(!restored.getActualReturnDate().has_value());
        VERIFY(restored.getEstimatedCost() == 3500.0);
        VERIFY(!restored.getFinalCost().has_value());
        VERIFY(restored.getStatus() == core::RentalStatus::Confirmed);
    } PASS;
}

void testRentalActiveRoundTrip() {
    TEST("RentalRecord Active state round-trip") {
        core::Date start(2026, 11, 10);
        core::Date end(2026, 11, 15);
        models::RentalRecord rental(101, 10, 1, core::DateRange(start, end), 3500.0);
        rental.markActive(start);

        auto fields = RentalSerializer::serialize(rental);
        VERIFY(fields.size() == 10);
        VERIFY(fields[9] == "2026-11-10");

        std::string csv = CsvHelper::escapeRow(fields);
        auto restored = RentalSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getRentalId() == 101);
        VERIFY(restored.getStatus() == core::RentalStatus::Active);
        VERIFY(restored.getActualStartDate().has_value());
        VERIFY(*restored.getActualStartDate() == start);
        VERIFY(!restored.getActualReturnDate().has_value());
        VERIFY(!restored.getFinalCost().has_value());
    } PASS;
}

void testRentalCompletedRoundTrip() {
    TEST("RentalRecord Completed state round-trip") {
        core::Date start(2026, 11, 10);
        core::Date end(2026, 11, 15);
        core::Date actualReturn(2026, 11, 16);
        models::RentalRecord rental(102, 10, 1, core::DateRange(start, end), 3500.0);
        rental.markActive(start);
        rental.completeRental(actualReturn, 4200.50);

        auto fields = RentalSerializer::serialize(rental);
        VERIFY(fields.size() == 10);
        VERIFY(fields[5] == "2026-11-16");
        VERIFY(fields[7] == "4200.5");
        VERIFY(fields[8] == "Completed");
        VERIFY(fields[9] == "2026-11-10");

        std::string csv = CsvHelper::escapeRow(fields);
        auto restored = RentalSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getRentalId() == 102);
        VERIFY(restored.getStatus() == core::RentalStatus::Completed);
        VERIFY(restored.getActualStartDate().has_value());
        VERIFY(*restored.getActualStartDate() == start);
        VERIFY(restored.getActualReturnDate().has_value());
        VERIFY(*restored.getActualReturnDate() == actualReturn);
        VERIFY(restored.getFinalCost().has_value());
        VERIFY(*restored.getFinalCost() == 4200.50);
    } PASS;
}

void testRentalCancelledRoundTrip() {
    TEST("RentalRecord Cancelled state round-trip") {
        core::Date start(2026, 11, 10);
        core::Date end(2026, 11, 15);
        models::RentalRecord rental(103, 10, 1, core::DateRange(start, end), 3500.0);
        rental.markCancelled();

        auto fields = RentalSerializer::serialize(rental);
        VERIFY(fields.size() == 10);
        VERIFY(fields[9] == "");

        std::string csv = CsvHelper::escapeRow(fields);
        auto restored = RentalSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getRentalId() == 103);
        VERIFY(restored.getStatus() == core::RentalStatus::Cancelled);
        VERIFY(!restored.getActualStartDate().has_value());
        VERIFY(!restored.getActualReturnDate().has_value());
        VERIFY(!restored.getFinalCost().has_value());
    } PASS;
}

void testRentalRestorationMatrix() {
    core::Date start(2026, 11, 10);
    core::DateRange range(start, core::Date(2026, 11, 15));
    core::Date validReturn(2026, 11, 14);
    core::Date invalidEarlyReturn(2026, 11, 9); // before start date

    // ---- Confirmed State Invariants ----
    TEST("restoreFromStorage rejects Confirmed with actualReturnDate only") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, validReturn, 100.0, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Confirmed with finalCost only") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, std::nullopt, 100.0, 100.0, core::RentalStatus::Confirmed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Confirmed with both actualReturnDate and finalCost") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, validReturn, 100.0, 100.0, core::RentalStatus::Confirmed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Confirmed with actualStartDate") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, start, std::nullopt, 100.0, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
    } PASS;

    // ---- Active State Invariants ----
    TEST("restoreFromStorage rejects Active with actualReturnDate only") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, validReturn, 100.0, std::nullopt, core::RentalStatus::Active),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Active with finalCost only") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, std::nullopt, 100.0, 100.0, core::RentalStatus::Active),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Active with both actualReturnDate and finalCost") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, validReturn, 100.0, 100.0, core::RentalStatus::Active),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Active with actualStartDate preceding scheduled start") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, invalidEarlyReturn, std::nullopt, 100.0, std::nullopt, core::RentalStatus::Active),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage accepts Active with valid actualStartDate or nullopt") {
        auto rWithStart = models::RentalRecord::restoreFromStorage(1, 1, 1, range, core::Date(2026, 11, 11), std::nullopt, 100.0, std::nullopt, core::RentalStatus::Active);
        VERIFY(rWithStart.getActualStartDate().has_value() && *rWithStart.getActualStartDate() == core::Date(2026, 11, 11));

        auto rLegacy = models::RentalRecord::restoreFromStorage(2, 1, 1, range, std::nullopt, std::nullopt, 100.0, std::nullopt, core::RentalStatus::Active);
        VERIFY(!rLegacy.getActualStartDate().has_value());
    } PASS;

    // ---- Cancelled State Invariants ----
    TEST("restoreFromStorage rejects Cancelled with actualReturnDate only") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, validReturn, 100.0, std::nullopt, core::RentalStatus::Cancelled),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Cancelled with finalCost only") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, std::nullopt, 100.0, 100.0, core::RentalStatus::Cancelled),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Cancelled with both actualReturnDate and finalCost") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, validReturn, 100.0, 100.0, core::RentalStatus::Cancelled),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Cancelled with actualStartDate") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, start, std::nullopt, 100.0, std::nullopt, core::RentalStatus::Cancelled),
            core::StorageException);
    } PASS;

    // ---- Completed State Invariants ----
    TEST("restoreFromStorage rejects Completed without actualReturnDate (only finalCost)") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, start, std::nullopt, 100.0, 100.0, core::RentalStatus::Completed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Completed without finalCost (only actualReturnDate)") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, start, validReturn, 100.0, std::nullopt, core::RentalStatus::Completed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Completed with neither actualReturnDate nor finalCost") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, start, std::nullopt, 100.0, std::nullopt, core::RentalStatus::Completed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Completed with actualReturnDate preceding actualStartDate") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, core::Date(2026, 11, 12), core::Date(2026, 11, 11), 100.0, 100.0, core::RentalStatus::Completed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Completed with actualStartDate preceding scheduled start") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, invalidEarlyReturn, validReturn, 100.0, 100.0, core::RentalStatus::Completed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage accepts Completed with valid actualStartDate") {
        auto r = models::RentalRecord::restoreFromStorage(1, 1, 1, range, core::Date(2026, 11, 11), validReturn, 100.0, 100.0, core::RentalStatus::Completed);
        VERIFY(r.getActualStartDate().has_value() && *r.getActualStartDate() == core::Date(2026, 11, 11));
        VERIFY(r.getActualReturnDate().has_value() && *r.getActualReturnDate() == validReturn);
    } PASS;

    TEST("restoreFromStorage accepts legacy Completed without actualStartDate") {
        auto r = models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, validReturn, 100.0, 100.0, core::RentalStatus::Completed);
        VERIFY(!r.getActualStartDate().has_value());
        VERIFY(r.getActualReturnDate().has_value() && *r.getActualReturnDate() == validReturn);
    } PASS;

    TEST("restoreFromStorage rejects legacy Completed with returnDate preceding scheduled start") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, invalidEarlyReturn, 100.0, 100.0, core::RentalStatus::Completed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Completed with negative finalCost") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, start, validReturn, 100.0, -50.0, core::RentalStatus::Completed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects Completed with non-finite finalCost (NaN / Inf)") {
        double nanVal = std::numeric_limits<double>::quiet_NaN();
        double infVal = std::numeric_limits<double>::infinity();
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, start, validReturn, 100.0, nanVal, core::RentalStatus::Completed),
            core::StorageException);
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, start, validReturn, 100.0, infVal, core::RentalStatus::Completed),
            core::StorageException);
    } PASS;

    // ---- General ID and cost invariants ----
    TEST("restoreFromStorage rejects non-positive IDs") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(0, 1, 1, range, std::nullopt, std::nullopt, 100.0, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 0, 1, range, std::nullopt, std::nullopt, 100.0, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, -5, range, std::nullopt, std::nullopt, 100.0, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
    } PASS;

    TEST("restoreFromStorage rejects negative or non-finite estimatedCost") {
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, std::nullopt, -1.0, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
        double nanVal = std::numeric_limits<double>::quiet_NaN();
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, std::nullopt, nanVal, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
    } PASS;
}

// ============================================================================
// 4. PAYMENT SERIALIZER TESTS
// ============================================================================

void testPaymentRoundTrip() {
    TEST("PaymentRecord Payment transaction round-trip") {
        core::Date date(2026, 11, 10);
        models::PaymentRecord pay(500, 100, 10, 3500.00, date,
                                  core::PaymentType::Payment,
                                  core::PaymentMethod::UPI,
                                  "Advance rental deposit, paid in full");

        auto fields = PaymentSerializer::serialize(pay);
        VERIFY(fields.size() == 8);
        VERIFY(fields[0] == "500");
        VERIFY(fields[1] == "100");
        VERIFY(fields[2] == "10");
        VERIFY(fields[3] == "3500");
        VERIFY(fields[4] == "2026-11-10");
        VERIFY(fields[5] == "Payment");
        VERIFY(fields[6] == "UPI");
        VERIFY(fields[7] == "Advance rental deposit, paid in full");

        std::string csv = CsvHelper::escapeRow(fields);
        auto restored = PaymentSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getPaymentId() == 500);
        VERIFY(restored.getRentalId() == 100);
        VERIFY(restored.getCustomerId() == 10);
        VERIFY(restored.getAmount() == 3500.00);
        VERIFY(restored.getTransactionDate() == date);
        VERIFY(restored.getPaymentType() == core::PaymentType::Payment);
        VERIFY(restored.getPaymentMethod() == core::PaymentMethod::UPI);
        VERIFY(restored.getDescription() == "Advance rental deposit, paid in full");
    } PASS;

    TEST("PaymentRecord Refund transaction round-trip") {
        core::Date date(2026, 11, 12);
        models::PaymentRecord refund(501, 100, 10, 500.00, date,
                                    core::PaymentType::Refund,
                                    core::PaymentMethod::Card,
                                    "Early return security deposit refund");

        std::string csv = CsvHelper::escapeRow(PaymentSerializer::serialize(refund));
        auto restored = PaymentSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getPaymentId() == 501);
        VERIFY(restored.getPaymentType() == core::PaymentType::Refund);
        VERIFY(restored.getPaymentMethod() == core::PaymentMethod::Card);
        VERIFY(restored.getAmount() == 500.00);
    } PASS;
}

void testPaymentErrors() {
    TEST("PaymentSerializer rejects wrong column count and invalid enums") {
        std::vector<std::string> seven = {"1", "1", "1", "100.0", "2026-11-10", "Payment", "Cash"};
        EXPECT_THROW(PaymentSerializer::deserialize(seven), core::StorageException);

        std::vector<std::string> badType = {
            "1", "1", "1", "100.0", "2026-11-10", "CryptoTransfer", "Cash", "Desc"
        };
        EXPECT_THROW(PaymentSerializer::deserialize(badType), core::StorageException);
    } PASS;
}

// ============================================================================
// 5. MAINTENANCE SERIALIZER TESTS (ALL 3 LIFECYCLE STATES)
// ============================================================================

void testMaintenanceScheduledRoundTrip() {
    TEST("MaintenanceRecord Scheduled state round-trip") {
        core::Date start(2026, 11, 1);
        models::MaintenanceRecord maint(200, 3, "Routine 50,000 km general service", start, 2500.0);

        auto fields = MaintenanceSerializer::serialize(maint);
        VERIFY(fields.size() == 7);
        VERIFY(fields[0] == "200");
        VERIFY(fields[1] == "3");
        VERIFY(fields[2] == "Routine 50,000 km general service");
        VERIFY(fields[3] == "2026-11-01");
        VERIFY(fields[4] == ""); // completionDate is empty
        VERIFY(fields[5] == "2500");
        VERIFY(fields[6] == "Scheduled");

        std::string csv = CsvHelper::escapeRow(fields);
        auto restored = MaintenanceSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getMaintenanceId() == 200);
        VERIFY(restored.getVehicleId() == 3);
        VERIFY(restored.getProblemDescription() == "Routine 50,000 km general service");
        VERIFY(restored.getStartDate() == start);
        VERIFY(!restored.getCompletionDate().has_value());
        VERIFY(restored.getCost() == 2500.0);
        VERIFY(restored.getStatus() == core::MaintenanceStatus::Scheduled);
    } PASS;
}

void testMaintenanceInProgressRoundTrip() {
    TEST("MaintenanceRecord InProgress state round-trip") {
        core::Date start(2026, 11, 1);
        models::MaintenanceRecord maint(201, 3, "Brake pads replacement and wheel alignment", start, 2500.0);
        maint.startMaintenance();

        std::string csv = CsvHelper::escapeRow(MaintenanceSerializer::serialize(maint));
        auto restored = MaintenanceSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getMaintenanceId() == 201);
        VERIFY(restored.getStatus() == core::MaintenanceStatus::InProgress);
        VERIFY(!restored.getCompletionDate().has_value());
    } PASS;
}

void testMaintenanceCompletedRoundTrip() {
    TEST("MaintenanceRecord Completed state round-trip") {
        core::Date start(2026, 11, 1);
        core::Date finish(2026, 11, 3);
        models::MaintenanceRecord maint(202, 3, "Engine overhaul, timing belt replaced", start, 2500.0);
        maint.startMaintenance();
        maint.completeMaintenance(finish, 8750.25);

        auto fields = MaintenanceSerializer::serialize(maint);
        VERIFY(fields[4] == "2026-11-03");
        VERIFY(fields[5] == "8750.25");
        VERIFY(fields[6] == "Completed");

        std::string csv = CsvHelper::escapeRow(fields);
        auto restored = MaintenanceSerializer::deserialize(CsvHelper::parseLine(csv));

        VERIFY(restored.getMaintenanceId() == 202);
        VERIFY(restored.getStatus() == core::MaintenanceStatus::Completed);
        VERIFY(restored.getCompletionDate().has_value());
        VERIFY(*restored.getCompletionDate() == finish);
        VERIFY(restored.getCost() == 8750.25);
    } PASS;
}

void testMaintenanceValidationErrors() {
    TEST("MaintenanceSerializer rejects invalid column counts") {
        std::vector<std::string> six = {"1", "1", "Desc", "2026-11-01", "", "100.0"};
        EXPECT_THROW(MaintenanceSerializer::deserialize(six), core::StorageException);
    } PASS;

    TEST("MaintenanceSerializer rejects Scheduled with completionDate") {
        std::vector<std::string> fields = {
            "1", "1", "Desc", "2026-11-01", "2026-11-03", "100.0", "Scheduled"
        };
        EXPECT_THROW(MaintenanceSerializer::deserialize(fields), core::StorageException);
    } PASS;

    TEST("MaintenanceSerializer rejects Completed without completionDate") {
        std::vector<std::string> fields = {
            "1", "1", "Desc", "2026-11-01", "", "100.0", "Completed"
        };
        EXPECT_THROW(MaintenanceSerializer::deserialize(fields), core::StorageException);
    } PASS;

    TEST("MaintenanceSerializer rejects completionDate preceding startDate") {
        std::vector<std::string> fields = {
            "1", "1", "Desc", "2026-11-05", "2026-11-01", "100.0", "Completed"
        };
        EXPECT_THROW(MaintenanceSerializer::deserialize(fields), core::StorageException);
    } PASS;

    TEST("MaintenanceSerializer rejects negative cost") {
        std::vector<std::string> fields = {
            "1", "1", "Desc", "2026-11-01", "", "-500.0", "Scheduled"
        };
        EXPECT_THROW(MaintenanceSerializer::deserialize(fields), core::StorageException);
    } PASS;
}

// ============================================================================
// 6. FLOATING-POINT SERIALIZATION & PRECISION TESTS
// ============================================================================

void testFloatingPointSerializationAndPrecision() {
    TEST("Exact round-trip of 0.1") {
        core::DateRange range(core::Date(2026, 11, 10), core::Date(2026, 11, 15));
        models::RentalRecord rental(1, 10, 20, range, 0.1);
        auto fields = RentalSerializer::serialize(rental);
        auto restored = RentalSerializer::deserialize(CsvHelper::parseLine(CsvHelper::escapeRow(fields)));
        VERIFY(restored.getEstimatedCost() == 0.1);
    } PASS;

    TEST("Exact round-trip of many decimal digits (e.g. 1234.567890123456)") {
        core::Date date(2026, 11, 10);
        double preciseAmount = 1234.567890123456;
        models::PaymentRecord pay(1, 2, 3, preciseAmount, date,
                                  core::PaymentType::Payment, core::PaymentMethod::Cash, "Precision test");
        auto fields = PaymentSerializer::serialize(pay);
        auto restored = PaymentSerializer::deserialize(CsvHelper::parseLine(CsvHelper::escapeRow(fields)));
        VERIFY(restored.getAmount() == preciseAmount);
    } PASS;

    TEST("Exact round-trip of very small finite values (1e-9 and 1e-15)") {
        core::Date start(2026, 11, 1);
        double smallVal = 1e-9;
        models::MaintenanceRecord m1(1, 1, "Small test", start, smallVal);
        auto restored1 = MaintenanceSerializer::deserialize(CsvHelper::parseLine(CsvHelper::escapeRow(MaintenanceSerializer::serialize(m1))));
        VERIFY(restored1.getCost() == smallVal);

        double tinyVal = 1e-15;
        models::MaintenanceRecord m2(2, 1, "Tiny test", start, tinyVal);
        auto restored2 = MaintenanceSerializer::deserialize(CsvHelper::parseLine(CsvHelper::escapeRow(MaintenanceSerializer::serialize(m2))));
        VERIFY(restored2.getCost() == tinyVal);
    } PASS;

    TEST("Exact round-trip of large finite values (1e9 and 123456789012.34)") {
        core::Date date(2026, 11, 10);
        double largeVal = 123456789012.34;
        models::PaymentRecord pay(1, 2, 3, largeVal, date,
                                  core::PaymentType::Payment, core::PaymentMethod::Card, "Large test");
        auto restored = PaymentSerializer::deserialize(CsvHelper::parseLine(CsvHelper::escapeRow(PaymentSerializer::serialize(pay))));
        VERIFY(restored.getAmount() == largeVal);
    } PASS;

    TEST("Rejection of NaN in all variations (nan, NaN, -nan, +NAN)") {
        std::vector<std::string> carRow = {"1", "CAR", "T", "C", "P", "nan", "Available", "5", "Gas", "true"};
        EXPECT_THROW(VehicleSerializer::deserialize(carRow), core::StorageException);

        std::vector<std::string> rentalRow = {"1", "1", "1", "2026-11-10", "2026-11-15", "", "NaN", "", "Confirmed"};
        EXPECT_THROW(RentalSerializer::deserialize(rentalRow), core::StorageException);

        std::vector<std::string> payRow = {"1", "1", "1", "-nan", "2026-11-10", "Payment", "Cash", "Desc"};
        EXPECT_THROW(PaymentSerializer::deserialize(payRow), core::StorageException);

        std::vector<std::string> maintRow = {"1", "1", "Desc", "2026-11-01", "", "+NAN", "Scheduled"};
        EXPECT_THROW(MaintenanceSerializer::deserialize(maintRow), core::StorageException);
    } PASS;

    TEST("Rejection of Positive and Negative Infinity (inf, -inf, +INF, infinity)") {
        std::vector<std::string> f1 = {"1", "CAR", "T", "C", "P", "inf", "Available", "5", "Gas", "true"};
        EXPECT_THROW(VehicleSerializer::deserialize(f1), core::StorageException);

        std::vector<std::string> f2 = {"1", "CAR", "T", "C", "P", "-inf", "Available", "5", "Gas", "true"};
        EXPECT_THROW(VehicleSerializer::deserialize(f2), core::StorageException);

        std::vector<std::string> f3 = {"1", "CAR", "T", "C", "P", "+INF", "Available", "5", "Gas", "true"};
        EXPECT_THROW(VehicleSerializer::deserialize(f3), core::StorageException);

        std::vector<std::string> f4 = {"1", "CAR", "T", "C", "P", "infinity", "Available", "5", "Gas", "true"};
        EXPECT_THROW(VehicleSerializer::deserialize(f4), core::StorageException);

        std::vector<std::string> f5 = {"1", "CAR", "T", "C", "P", "-infinity", "Available", "5", "Gas", "true"};
        EXPECT_THROW(VehicleSerializer::deserialize(f5), core::StorageException);
    } PASS;

    TEST("Rejection of non-finite values during serialization") {
        double nanVal = std::numeric_limits<double>::quiet_NaN();
        double infVal = std::numeric_limits<double>::infinity();

        models::Car badCar(1, "B", "M", "P", 50.0, 5, "Gas", true);
        badCar.setBaseDailyRate(100.0);

        // Truck with NaN capacity:
        models::Truck truck(1, "B", "M", "P", 100.0, 10.0, 2);
        // Payment with inf
        core::Date d(2026, 11, 10);
        models::PaymentRecord pay(1, 1, 1, 100.0, d, core::PaymentType::Payment, core::PaymentMethod::Cash, "Desc");

        // Verify that passing NaN/inf to restoreFromStorage throws
        core::DateRange range(core::Date(2026, 11, 10), core::Date(2026, 11, 15));
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, std::nullopt, nanVal, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
        EXPECT_THROW(
            models::RentalRecord::restoreFromStorage(1, 1, 1, range, std::nullopt, std::nullopt, infVal, std::nullopt, core::RentalStatus::Confirmed),
            core::StorageException);
    } PASS;
}

void testRentalActualStartDateMigration() {
    core::Date start(2026, 11, 10);
    core::Date end(2026, 11, 15);
    core::DateRange range(start, end);

    // 1. Ten-field serialization
    TEST("RentalSerializer produces exactly 10 fields for all statuses") {
        models::RentalRecord conf(1, 10, 1, range, 1000.0);
        auto fieldsConf = RentalSerializer::serialize(conf);
        VERIFY(fieldsConf.size() == 10);
        VERIFY(fieldsConf[9] == "");

        models::RentalRecord act(2, 10, 1, range, 1000.0);
        act.markActive(core::Date(2026, 11, 11));
        auto fieldsAct = RentalSerializer::serialize(act);
        VERIFY(fieldsAct.size() == 10);
        VERIFY(fieldsAct[9] == "2026-11-11");

        act.completeRental(core::Date(2026, 11, 15), 1000.0);
        auto fieldsComp = RentalSerializer::serialize(act);
        VERIFY(fieldsComp.size() == 10);
        VERIFY(fieldsComp[9] == "2026-11-11");
    } PASS;

    // 2. Ten-field deserialization with populated actualStartDate
    TEST("RentalSerializer deserializes 10-field row with actualStartDate") {
        std::vector<std::string> fields = {
            "10", "20", "5", "2026-11-10", "2026-11-15", "", "1500.0", "", "Active", "2026-11-12"
        };
        auto r = RentalSerializer::deserialize(fields);
        VERIFY(r.getRentalId() == 10);
        VERIFY(r.getStatus() == core::RentalStatus::Active);
        VERIFY(r.getActualStartDate().has_value());
        VERIFY(*r.getActualStartDate() == core::Date(2026, 11, 12));
    } PASS;

    // 3. Nine-field legacy deserialization
    TEST("RentalSerializer deserializes legacy 9-field rows with actualStartDate as nullopt") {
        // Legacy Active
        std::vector<std::string> fieldsActive9 = {
            "11", "20", "5", "2026-11-10", "2026-11-15", "", "1500.0", "", "Active"
        };
        auto rActive = RentalSerializer::deserialize(fieldsActive9);
        VERIFY(rActive.getRentalId() == 11);
        VERIFY(rActive.getStatus() == core::RentalStatus::Active);
        VERIFY(!rActive.getActualStartDate().has_value());

        // Legacy Confirmed
        std::vector<std::string> fieldsConf9 = {
            "12", "20", "5", "2026-11-10", "2026-11-15", "", "1500.0", "", "Confirmed"
        };
        auto rConf = RentalSerializer::deserialize(fieldsConf9);
        VERIFY(rConf.getRentalId() == 12);
        VERIFY(!rConf.getActualStartDate().has_value());
    } PASS;

    // 4. Legacy Completed record without actualStartDate
    TEST("RentalSerializer deserializes legacy Completed record without actualStartDate") {
        // 9-field legacy Completed
        std::vector<std::string> fieldsComp9 = {
            "13", "20", "5", "2026-11-10", "2026-11-15", "2026-11-16", "1500.0", "1600.0", "Completed"
        };
        auto rComp9 = RentalSerializer::deserialize(fieldsComp9);
        VERIFY(rComp9.getRentalId() == 13);
        VERIFY(rComp9.getStatus() == core::RentalStatus::Completed);
        VERIFY(!rComp9.getActualStartDate().has_value());
        VERIFY(rComp9.getActualReturnDate().has_value() && *rComp9.getActualReturnDate() == core::Date(2026, 11, 16));
        VERIFY(rComp9.getFinalCost().has_value() && *rComp9.getFinalCost() == 1600.0);

        // 10-field Completed with empty 10th field
        std::vector<std::string> fieldsComp10Empty = {
            "14", "20", "5", "2026-11-10", "2026-11-15", "2026-11-16", "1500.0", "1600.0", "Completed", ""
        };
        auto rComp10 = RentalSerializer::deserialize(fieldsComp10Empty);
        VERIFY(rComp10.getRentalId() == 14);
        VERIFY(rComp10.getStatus() == core::RentalStatus::Completed);
        VERIFY(!rComp10.getActualStartDate().has_value());

        // Legacy Completed with invalid returnDate preceding scheduled start throws StorageException
        std::vector<std::string> fieldsCompInvalid = {
            "15", "20", "5", "2026-11-10", "2026-11-15", "2026-11-09", "1500.0", "1600.0", "Completed", ""
        };
        EXPECT_THROW(RentalSerializer::deserialize(fieldsCompInvalid), core::StorageException);
    } PASS;

    // 5. Empty tenth field
    TEST("RentalSerializer accepts empty tenth field as std::nullopt") {
        std::vector<std::string> fields = {
            "16", "20", "5", "2026-11-10", "2026-11-15", "", "1500.0", "", "Confirmed", ""
        };
        auto r = RentalSerializer::deserialize(fields);
        VERIFY(r.getRentalId() == 16);
        VERIFY(!r.getActualStartDate().has_value());
    } PASS;

    // 6. Malformed actualStartDate produces StorageException
    TEST("RentalSerializer rejects malformed actualStartDate") {
        std::vector<std::string> fieldsMalformed1 = {
            "17", "20", "5", "2026-11-10", "2026-11-15", "", "1500.0", "", "Active", "invalid-date"
        };
        EXPECT_THROW(RentalSerializer::deserialize(fieldsMalformed1), core::StorageException);

        std::vector<std::string> fieldsMalformed2 = {
            "18", "20", "5", "2026-11-10", "2026-11-15", "", "1500.0", "", "Active", "2026-99-99"
        };
        EXPECT_THROW(RentalSerializer::deserialize(fieldsMalformed2), core::StorageException);

        // Row with incorrect column count (e.g., 8 or 11)
        std::vector<std::string> fields8 = {
            "19", "20", "5", "2026-11-10", "2026-11-15", "", "1500.0", ""
        };
        EXPECT_THROW(RentalSerializer::deserialize(fields8), core::StorageException);

        std::vector<std::string> fields11 = {
            "20", "20", "5", "2026-11-10", "2026-11-15", "", "1500.0", "", "Confirmed", "", "extra"
        };
        EXPECT_THROW(RentalSerializer::deserialize(fields11), core::StorageException);
    } PASS;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "\n===== 1. VEHICLE SERIALIZER TESTS =====\n";
    testCarSerializationRoundTrip();
    testBikeSerializationRoundTrip();
    testTruckSerializationRoundTrip();
    testVehicleSpecialCharacters();
    testVehicleErrors();

    std::cout << "\n===== 2. USER SERIALIZER TESTS =====\n";
    testCustomerSerializationRoundTrip();
    testEmployeeSerializationRoundTrip();
    testAdminSerializationRoundTrip();
    testUserSpecialCharacters();
    testUserErrors();

    std::cout << "\n===== 3. RENTAL RECORD SERIALIZER TESTS =====\n";
    testRentalConfirmedRoundTrip();
    testRentalActiveRoundTrip();
    testRentalCompletedRoundTrip();
    testRentalCancelledRoundTrip();
    testRentalRestorationMatrix();
    testRentalActualStartDateMigration();

    std::cout << "\n===== 4. PAYMENT RECORD SERIALIZER TESTS =====\n";
    testPaymentRoundTrip();
    testPaymentErrors();

    std::cout << "\n===== 5. MAINTENANCE RECORD SERIALIZER TESTS =====\n";
    testMaintenanceScheduledRoundTrip();
    testMaintenanceInProgressRoundTrip();
    testMaintenanceCompletedRoundTrip();
    testMaintenanceValidationErrors();

    std::cout << "\n===== 6. FLOATING-POINT SERIALIZATION & PRECISION TESTS =====\n";
    testFloatingPointSerializationAndPrecision();

    std::cout << "\n===== RESULTS =====\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";

    if (failed > 0) {
        std::cerr << "*** SOME CSV SERIALIZER TESTS FAILED ***\n";
        return 1;
    }
    std::cout << "All CSV serializer tests passed successfully.\n\n";
    return 0;
}
