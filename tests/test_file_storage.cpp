/**
 * @file test_file_storage.cpp
 * @brief Comprehensive tests for FileStorage persistence coordinator.
 *
 * Verifies:
 * 1. Configurable data directory with isolated sandbox testing
 * 2. Missing files graceful handling
 * 3. End-to-end full save & load round-trip across all 5 datasets
 * 4. Deterministic sorting by integer ID ascending
 * 5. Robust syntax error handling, duplicate ID rejection, line-number tracking
 * 6. Referential integrity validation & Quarantine:
 *    - Rental -> Vehicle
 *    - Rental -> Customer (rejects Employee/Admin)
 *    - Payment -> Rental
 *    - Payment -> Customer
 *    - Payment customerId == Rental customerId matching
 *    - Maintenance -> Vehicle
 * 7. Safe file replacement (Backup-Swap protocol) & original-file preservation
 */

#include "fleetflow/storage/FileStorage.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Bike.hpp"
#include "fleetflow/models/Truck.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/Employee.hpp"
#include "fleetflow/models/Admin.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
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

// Helper for clean sandbox directory management
static const std::filesystem::path SANDBOX_DIR = "sandbox_test_storage";

void cleanSandbox() {
    std::error_code ec;
    std::filesystem::remove_all(SANDBOX_DIR, ec);
}

// ============================================================================
// 1. CONFIGURABLE PATH & MISSING FILES TESTS
// ============================================================================

void testMissingFilesHandling() {
    TEST("Missing files return empty datasets without throwing") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);

        FileStorage storage(SANDBOX_DIR);
        VERIFY(storage.getDataDirectory() == SANDBOX_DIR);

        Repository<models::Vehicle> vRepo;
        Repository<models::User> uRepo;
        Repository<models::RentalRecord> rRepo;
        Repository<models::PaymentRecord> pRepo;
        Repository<models::MaintenanceRecord> mRepo;

        auto report = storage.loadAll(vRepo, uRepo, rRepo, pRepo, mRepo);

        VERIFY(report.loadedVehicles == 0);
        VERIFY(report.loadedUsers == 0);
        VERIFY(report.loadedRentals == 0);
        VERIFY(report.loadedPayments == 0);
        VERIFY(report.loadedMaintenance == 0);
        VERIFY(vRepo.empty());
        VERIFY(uRepo.empty());
        VERIFY(rRepo.empty());
        VERIFY(pRepo.empty());
        VERIFY(mRepo.empty());
        VERIFY(!report.hasWarnings());

        cleanSandbox();
    } PASS;
}

// ============================================================================
// 2. FULL SAVE & LOAD ROUND-TRIP TESTS
// ============================================================================

void testFullSaveAndLoadRoundTrip() {
    TEST("Full saveAll and loadAll round-trip across all 5 datasets") {
        cleanSandbox();
        FileStorage storage(SANDBOX_DIR);

        Repository<models::Vehicle> vRepo;
        Repository<models::User> uRepo;
        Repository<models::RentalRecord> rRepo;
        Repository<models::PaymentRecord> pRepo;
        Repository<models::MaintenanceRecord> mRepo;

        // Populate Vehicles
        vRepo.add(std::make_unique<models::Car>(1, "Toyota", "Camry", "KA-01-AB-1234", 4500.0, 5, "Petrol", true));
        vRepo.add(std::make_unique<models::Bike>(2, "Royal Enfield", "Hunter 350", "DL-04-XY-9999", 1200.0, 349, true));
        vRepo.add(std::make_unique<models::Truck>(3, "Volvo", "FH16", "MH-12-TR-7777", 8500.0, 16.5, 4));

        // Populate Users
        uRepo.add(std::make_unique<models::Customer>(10, "rahul_k", "Rahul Kumar", "rahul@test.com", "DL-999", "+919876543210"));
        uRepo.add(std::make_unique<models::Employee>(20, "priya_s", "Priya Sharma", "priya@fleet.in", "EMP-101", "Ops"));
        uRepo.add(std::make_unique<models::Admin>(30, "aditya_a", "Aditya Suryan", "admin@fleet.in", 5, "HQ"));

        // Populate Rentals
        core::DateRange range1(core::Date(2026, 11, 10), core::Date(2026, 11, 15));
        auto r1 = std::make_unique<models::RentalRecord>(100, 10, 1, range1, 4500.0);
        r1->markActive(core::Date(2026, 11, 10));
        r1->completeRental(core::Date(2026, 11, 15), 4500.0);
        rRepo.add(r1);

        core::DateRange range2(core::Date(2026, 11, 20), core::Date(2026, 11, 25));
        rRepo.add(std::make_unique<models::RentalRecord>(101, 10, 2, range2, 1200.0));

        // Populate Payments
        pRepo.add(std::make_unique<models::PaymentRecord>(
            500, 100, 10, 4500.0, core::Date(2026, 11, 10), core::PaymentType::Payment, core::PaymentMethod::UPI, "Rental booking"));

        // Populate Maintenance
        auto m1 = std::make_unique<models::MaintenanceRecord>(200, 3, "Brake service", core::Date(2026, 11, 1), 2500.0);
        m1->startMaintenance();
        m1->completeMaintenance(core::Date(2026, 11, 3), 3200.0);
        mRepo.add(m1);

        // Save all datasets
        storage.saveAll(vRepo, uRepo, rRepo, pRepo, mRepo);

        // Verify CSV files exist on disk
        VERIFY(std::filesystem::exists(SANDBOX_DIR / "vehicles.csv"));
        VERIFY(std::filesystem::exists(SANDBOX_DIR / "users.csv"));
        VERIFY(std::filesystem::exists(SANDBOX_DIR / "rentals.csv"));
        VERIFY(std::filesystem::exists(SANDBOX_DIR / "payments.csv"));
        VERIFY(std::filesystem::exists(SANDBOX_DIR / "maintenance.csv"));

        // Load into fresh repositories
        Repository<models::Vehicle> loadedV;
        Repository<models::User> loadedU;
        Repository<models::RentalRecord> loadedR;
        Repository<models::PaymentRecord> loadedP;
        Repository<models::MaintenanceRecord> loadedM;

        auto report = storage.loadAll(loadedV, loadedU, loadedR, loadedP, loadedM);

        VERIFY(report.loadedVehicles == 3);
        VERIFY(report.loadedUsers == 3);
        VERIFY(report.loadedRentals == 2);
        VERIFY(report.loadedPayments == 1);
        VERIFY(report.loadedMaintenance == 1);
        VERIFY(!report.hasWarnings());

        // Verify restored contents & types
        VERIFY(loadedV.getById(1)->getVehicleType() == "Car");
        VERIFY(loadedV.getById(2)->getVehicleType() == "Bike");
        VERIFY(loadedV.getById(3)->getVehicleType() == "Truck");
        VERIFY(loadedU.getById(10)->getRoleName() == "Customer");
        VERIFY(loadedU.getById(20)->getRoleName() == "Employee");
        VERIFY(loadedU.getById(30)->getRoleName() == "Admin");

        auto* restoredR1 = loadedR.getById(100);
        VERIFY(restoredR1->getStatus() == core::RentalStatus::Completed);
        VERIFY(restoredR1->getActualStartDate().has_value() && *restoredR1->getActualStartDate() == core::Date(2026, 11, 10));
        VERIFY(restoredR1->getActualReturnDate().has_value());
        VERIFY(restoredR1->getFinalCost().has_value());

        auto* restoredM1 = loadedM.getById(200);
        VERIFY(restoredM1->getStatus() == core::MaintenanceStatus::Completed);
        VERIFY(restoredM1->getCost() == 3200.0);

        cleanSandbox();
    } PASS;
}

// ============================================================================
// 3. DETERMINISTIC SORTING TEST
// ============================================================================

void testDeterministicSorting() {
    TEST("Datasets are sorted by primary integer ID in ascending order") {
        cleanSandbox();
        FileStorage storage(SANDBOX_DIR);

        Repository<models::Vehicle> vRepo;
        // Insert out of order: 50, 10, 30, 2
        vRepo.add(std::make_unique<models::Car>(50, "C", "M", "P", 10.0, 5, "Gas", true));
        vRepo.add(std::make_unique<models::Car>(10, "C", "M", "P", 10.0, 5, "Gas", true));
        vRepo.add(std::make_unique<models::Car>(30, "C", "M", "P", 10.0, 5, "Gas", true));
        vRepo.add(std::make_unique<models::Car>(2, "C", "M", "P", 10.0, 5, "Gas", true));

        storage.saveVehicles(vRepo);

        // Read file line by line
        std::ifstream ifs(SANDBOX_DIR / "vehicles.csv");
        std::string header, l1, l2, l3, l4;
        std::getline(ifs, header);
        std::getline(ifs, l1);
        std::getline(ifs, l2);
        std::getline(ifs, l3);
        std::getline(ifs, l4);

        VERIFY(l1.rfind("2,", 0) == 0);
        VERIFY(l2.rfind("10,", 0) == 0);
        VERIFY(l3.rfind("30,", 0) == 0);
        VERIFY(l4.rfind("50,", 0) == 0);

        cleanSandbox();
    } PASS;
}

// ============================================================================
// 4. SYNTAX ERRORS & CORRUPTED ROW ISOLATION
// ============================================================================

void testSyntaxWarningsAndCorruptedRows() {
    TEST("Corrupted rows are skipped with warnings and valid rows are loaded") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);

        // Write a vehicles.csv with:
        // Line 1: Header
        // Line 2: Valid Car ID 1
        // Line 3: Corrupted row (missing columns)
        // Line 4: Valid Bike ID 2
        // Line 5: Corrupted row (non-numeric rate)
        // Line 6: Duplicate Vehicle ID 1
        // Line 7: Valid Truck ID 3
        std::ofstream ofs(SANDBOX_DIR / "vehicles.csv");
        ofs << "id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3\n"
            << "1,CAR,Toyota,Camry,ABC-123,50.0,Available,5,Petrol,true\n"
            << "invalid,row,too,few,fields\n"
            << "2,BIKE,Honda,CBR,XYZ-999,30.0,Available,150,true,\n"
            << "4,CAR,Brand,Model,PLT-001,not_a_number,Available,5,Gas,true\n"
            << "1,CAR,Toyota,Duplicate,ABC-123,50.0,Available,5,Petrol,true\n"
            << "3,TRUCK,Volvo,FH16,TRK-001,100.0,Available,10.0,2,\n";
        ofs.close();

        FileStorage storage(SANDBOX_DIR);
        Repository<models::Vehicle> vRepo;
        std::vector<std::string> warnings;
        std::size_t loaded = storage.loadVehicles(vRepo, warnings);

        VERIFY(loaded == 3);
        VERIFY(vRepo.size() == 3);
        VERIFY(vRepo.exists(1));
        VERIFY(vRepo.exists(2));
        VERIFY(vRepo.exists(3));
        VERIFY(warnings.size() == 3);

        // Check that filename and line numbers are present in diagnostics
        VERIFY(warnings[0].find("vehicles.csv:line 3") != std::string::npos);
        VERIFY(warnings[1].find("vehicles.csv:line 5") != std::string::npos);
        VERIFY(warnings[2].find("vehicles.csv:line 6") != std::string::npos);

        cleanSandbox();
    } PASS;
}

// ============================================================================
// 5. REFERENTIAL INTEGRITY VALIDATION & QUARANTINE TESTS
// ============================================================================

void testReferentialIntegrityQuarantine() {
    TEST("Orphan and inconsistent records are quarantined and excluded from repos") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);

        // 1. Vehicles: ID 1 and ID 2
        {
            std::ofstream ofs(SANDBOX_DIR / "vehicles.csv");
            ofs << "id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3\n"
                << "1,CAR,Toyota,Camry,ABC-123,50.0,Available,5,Petrol,true\n"
                << "2,BIKE,Honda,CBR,XYZ-999,30.0,Available,150,true,\n";
        }

        // 2. Users: Customer ID 10, Employee ID 20
        {
            std::ofstream ofs(SANDBOX_DIR / "users.csv");
            ofs << "id,role,username,fullName,email,extra1,extra2\n"
                << "10,CUSTOMER,cust_a,Customer A,a@test.com,DL-111,+919999999999\n"
                << "20,EMPLOYEE,emp_b,Employee B,b@test.com,EMP-222,Operations\n";
        }

        // 3. Rentals:
        // - Rental 100: valid (cust 10, vehicle 1)
        // - Rental 101: invalid orphan vehicle (cust 10, vehicle 999) -> QUARANTINE
        // - Rental 102: invalid non-customer user (cust 20 is Employee, vehicle 2) -> QUARANTINE
        // - Rental 103: invalid orphan customer (cust 888, vehicle 1) -> QUARANTINE
        {
            std::ofstream ofs(SANDBOX_DIR / "rentals.csv");
            ofs << "rentalId,customerId,vehicleId,startDate,scheduledEndDate,actualReturnDate,estimatedCost,finalCost,status\n"
                << "100,10,1,2026-11-10,2026-11-15,,100.0,,Confirmed\n"
                << "101,10,999,2026-11-10,2026-11-15,,100.0,,Confirmed\n"
                << "102,20,2,2026-11-10,2026-11-15,,100.0,,Confirmed\n"
                << "103,888,1,2026-11-10,2026-11-15,,100.0,,Confirmed\n";
        }

        // 4. Payments:
        // - Payment 500: valid (rental 100, cust 10)
        // - Payment 501: invalid orphan rental (rental 999, cust 10) -> QUARANTINE
        // - Payment 502: invalid customer mismatch (rental 100, cust 20 != rental.cust 10) -> QUARANTINE
        {
            std::ofstream ofs(SANDBOX_DIR / "payments.csv");
            ofs << "paymentId,rentalId,customerId,amount,date,type,method,description\n"
                << "500,100,10,100.0,2026-11-10,Payment,Cash,Valid payment\n"
                << "501,999,10,100.0,2026-11-10,Payment,Cash,Orphan rental\n"
                << "502,100,20,100.0,2026-11-10,Payment,Cash,Customer mismatch\n";
        }

        // 5. Maintenance:
        // - Maintenance 300: valid (vehicle 1)
        // - Maintenance 301: invalid orphan vehicle (vehicle 888) -> QUARANTINE
        {
            std::ofstream ofs(SANDBOX_DIR / "maintenance.csv");
            ofs << "maintenanceId,vehicleId,problemDescription,startDate,completionDate,cost,status\n"
                << "300,1,Oil change,2026-11-01,,100.0,Scheduled\n"
                << "301,888,Engine knock,2026-11-01,,100.0,Scheduled\n";
        }

        FileStorage storage(SANDBOX_DIR);
        Repository<models::Vehicle> vRepo;
        Repository<models::User> uRepo;
        Repository<models::RentalRecord> rRepo;
        Repository<models::PaymentRecord> pRepo;
        Repository<models::MaintenanceRecord> mRepo;

        auto report = storage.loadAll(vRepo, uRepo, rRepo, pRepo, mRepo);

        // Verify that operational repositories contain ONLY valid records
        VERIFY(vRepo.size() == 2);
        VERIFY(uRepo.size() == 2);
        VERIFY(rRepo.size() == 1); // Only Rental 100 admitted
        VERIFY(pRepo.size() == 1); // Only Payment 500 admitted
        VERIFY(mRepo.size() == 1); // Only Maintenance 300 admitted

        VERIFY(rRepo.exists(100));
        VERIFY(!rRepo.exists(101));
        VERIFY(!rRepo.exists(102));
        VERIFY(!rRepo.exists(103));

        VERIFY(pRepo.exists(500));
        VERIFY(!pRepo.exists(501));
        VERIFY(!pRepo.exists(502));

        VERIFY(mRepo.exists(300));
        VERIFY(!mRepo.exists(301));

        // Verify quarantine diagnostics in report
        VERIFY(report.quarantined.size() == 6);

        cleanSandbox();
    } PASS;
}

// ============================================================================
// 6. SAFE FILE REPLACEMENT & ORIGINAL-FILE PRESERVATION TESTS
// ============================================================================

void testSafeFileReplacementAndPreservation() {
    TEST("Safe write protocol creates destination file cleanly") {
        cleanSandbox();
        std::filesystem::path target = SANDBOX_DIR / "test.txt";
        FileStorage::safeWriteFile(target, {"line1", "line2"});

        VERIFY(std::filesystem::exists(target));
        VERIFY(!std::filesystem::exists(SANDBOX_DIR / "test.txt.tmp"));
        VERIFY(!std::filesystem::exists(SANDBOX_DIR / "test.txt.bak"));

        std::ifstream ifs(target);
        std::string a, b;
        std::getline(ifs, a);
        std::getline(ifs, b);
        VERIFY(a == "line1");
        VERIFY(b == "line2");

        cleanSandbox();
    } PASS;

    TEST("Safe write preserves original file if replacement fails") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);
        std::filesystem::path target = SANDBOX_DIR / "original.txt";

        // Create original file
        FileStorage::safeWriteFile(target, {"ORIGINAL CONTENT"});
        VERIFY(std::filesystem::exists(target));

        // On Windows, holding an open file handle causes rename to fail with sharing violation
        {
            std::ofstream lock(target, std::ios::in | std::ios::out);
            VERIFY(lock.is_open());
            EXPECT_THROW(FileStorage::safeWriteFile(target, {"NEW CONTENT"}), core::StorageException);
        }

        // Verify original file is still 100% intact
        std::ifstream ifs(target);
        std::string content;
        std::getline(ifs, content);
        VERIFY(content == "ORIGINAL CONTENT");

        cleanSandbox();
    } PASS;
}

// ============================================================================
// 7. TRANSACTIONAL LOADING & CRASH RECOVERY TESTS
// ============================================================================

void testTransactionalLoadingPreservesOperationalRepositories() {
    TEST("Operational repositories remain untouched if fatal error occurs during loadAll") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);

        // Create valid vehicles.csv, users.csv, rentals.csv, payments.csv
        {
            std::ofstream ofs(SANDBOX_DIR / "vehicles.csv");
            ofs << "id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3\n"
                << "1,CAR,Toyota,Camry,ABC-123,50.0,Available,5,Petrol,true\n";
        }
        {
            std::ofstream ofs(SANDBOX_DIR / "users.csv");
            ofs << "id,role,username,fullName,email,extra1,extra2\n"
                << "10,CUSTOMER,cust_a,Customer A,a@test.com,DL-111,+919999999999\n";
        }
        {
            std::ofstream ofs(SANDBOX_DIR / "rentals.csv");
            ofs << "rentalId,customerId,vehicleId,startDate,scheduledEndDate,actualReturnDate,estimatedCost,finalCost,status\n"
                << "100,10,1,2026-11-10,2026-11-15,,100.0,,Confirmed\n";
        }
        {
            std::ofstream ofs(SANDBOX_DIR / "payments.csv");
            ofs << "paymentId,rentalId,customerId,amount,date,type,method,description\n"
                << "500,100,10,100.0,2026-11-10,Payment,Cash,Deposit\n";
        }
        // Create maintenance.csv as a directory to force an unreadable filesystem error on maintenance load
        std::filesystem::create_directories(SANDBOX_DIR / "maintenance.csv");

        FileStorage storage(SANDBOX_DIR);

        // Pre-populate operational repositories with initial sentinel data
        Repository<models::Vehicle> vRepo;
        Repository<models::User> uRepo;
        Repository<models::RentalRecord> rRepo;
        Repository<models::PaymentRecord> pRepo;
        Repository<models::MaintenanceRecord> mRepo;

        vRepo.add(std::make_unique<models::Bike>(999, "OriginalBike", "M", "P-999", 20.0, 150, true));
        uRepo.add(std::make_unique<models::Customer>(888, "orig_user", "Orig User", "u@test.com", "DL-0", "+910000000000"));

        // Verify loadAll throws StorageException identifying maintenance
        bool caught = false;
        try {
            storage.loadAll(vRepo, uRepo, rRepo, pRepo, mRepo);
        } catch (const core::StorageException& ex) {
            caught = true;
            std::string msg = ex.what();
            VERIFY(msg.find("maintenance.csv") != std::string::npos);
        }
        VERIFY(caught);

        // Verify operational repositories were NOT overwritten: sentinel objects remain intact!
        VERIFY(vRepo.size() == 1);
        VERIFY(vRepo.exists(999));
        VERIFY(uRepo.size() == 1);
        VERIFY(uRepo.exists(888));
        VERIFY(rRepo.empty());
        VERIFY(pRepo.empty());
        VERIFY(mRepo.empty());

        cleanSandbox();
    } PASS;
}

void testCrashRecoveryScenarios() {
    TEST("Crash Recovery Scenario A: Dangling .tmp file with valid destination is cleaned up") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);
        std::filesystem::path target = SANDBOX_DIR / "vehicles.csv";
        std::filesystem::path tmp = SANDBOX_DIR / "vehicles.csv.tmp";

        // Valid destination
        {
            std::ofstream ofs(target);
            ofs << "id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3\n"
                << "1,CAR,Toyota,Camry,ABC-123,50.0,Available,5,Petrol,true\n";
        }
        // Incomplete / abandoned temp file
        {
            std::ofstream ofs(tmp);
            ofs << "incomplete temp data";
        }

        FileStorage storage(SANDBOX_DIR);
        Repository<models::Vehicle> repo;
        std::vector<std::string> warnings;
        std::size_t loaded = storage.loadVehicles(repo, warnings);

        VERIFY(loaded == 1);
        VERIFY(repo.exists(1));
        // Temporary file should be cleaned up
        VERIFY(!std::filesystem::exists(tmp));
        VERIFY(std::filesystem::exists(target));

        cleanSandbox();
    } PASS;

    TEST("Crash Recovery Scenario B: Missing target restored from .bak") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);
        std::filesystem::path target = SANDBOX_DIR / "vehicles.csv";
        std::filesystem::path bak = SANDBOX_DIR / "vehicles.csv.bak";
        std::filesystem::path tmp = SANDBOX_DIR / "vehicles.csv.tmp";

        // Target does not exist, backup holds original data
        {
            std::ofstream ofs(bak);
            ofs << "id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3\n"
                << "2,BIKE,Honda,CBR,XYZ-999,30.0,Available,150,true,\n";
        }
        {
            std::ofstream ofs(tmp);
            ofs << "crashed temp write";
        }

        FileStorage storage(SANDBOX_DIR);
        Repository<models::Vehicle> repo;
        std::vector<std::string> warnings;
        std::size_t loaded = storage.loadVehicles(repo, warnings);

        VERIFY(loaded == 1);
        VERIFY(repo.exists(2));
        // Destination restored
        VERIFY(std::filesystem::exists(target));

        cleanSandbox();
    } PASS;

    TEST("Crash Recovery Scenario C: Valid target and .bak both present preserves .bak without blind deletion") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);
        std::filesystem::path target = SANDBOX_DIR / "vehicles.csv";
        std::filesystem::path bak = SANDBOX_DIR / "vehicles.csv.bak";

        {
            std::ofstream ofs(target);
            ofs << "id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3\n"
                << "1,CAR,Toyota,Camry,ABC-123,50.0,Available,5,Petrol,true\n";
        }
        {
            std::ofstream ofs(bak);
            ofs << "id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3\n"
                << "99,CAR,Old,Backup,BAK-001,10.0,Available,5,Gas,true\n";
        }

        FileStorage storage(SANDBOX_DIR);
        Repository<models::Vehicle> repo;
        std::vector<std::string> warnings;
        std::size_t loaded = storage.loadVehicles(repo, warnings);

        VERIFY(loaded == 1);
        VERIFY(repo.exists(1));
        // Backup is preserved and NOT blindly deleted on startup/load
        VERIFY(std::filesystem::exists(bak));

        cleanSandbox();
    } PASS;

    TEST("Crash Recovery Scenario D: 0-byte truncated target restored from non-empty .bak") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);
        std::filesystem::path target = SANDBOX_DIR / "vehicles.csv";
        std::filesystem::path bak = SANDBOX_DIR / "vehicles.csv.bak";

        // Target truncated to 0 bytes
        {
            std::ofstream ofs(target);
        }
        VERIFY(std::filesystem::file_size(target) == 0);

        // Backup has valid content
        {
            std::ofstream ofs(bak);
            ofs << "id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3\n"
                << "3,TRUCK,Volvo,FH16,TRK-001,100.0,Available,10.0,2,\n";
        }

        FileStorage storage(SANDBOX_DIR);
        Repository<models::Vehicle> repo;
        std::vector<std::string> warnings;
        std::size_t loaded = storage.loadVehicles(repo, warnings);

        VERIFY(loaded == 1);
        VERIFY(repo.exists(3));
        VERIFY(std::filesystem::file_size(target) > 0);

        cleanSandbox();
    } PASS;
}

// ============================================================================
// 8. MULTI-FILE SAVE LIMITATION VERIFICATION
// ============================================================================

void testMultiFileSaveLimitations() {
    TEST("Five-file save failure on 4th dataset preserves previous datasets but does not rollback earlier files") {
        cleanSandbox();
        FileStorage storage(SANDBOX_DIR);

        // Initial datasets on disk (v1)
        Repository<models::Vehicle> vRepo;
        Repository<models::User> uRepo;
        Repository<models::RentalRecord> rRepo;
        Repository<models::PaymentRecord> pRepo;
        Repository<models::MaintenanceRecord> mRepo;

        vRepo.add(std::make_unique<models::Car>(1, "Toyota", "Camry", "ABC-123", 50.0, 5, "Petrol", true));
        uRepo.add(std::make_unique<models::Customer>(10, "cust", "Customer", "c@test.com", "DL-1", "+919999999999"));
        rRepo.add(std::make_unique<models::RentalRecord>(100, 10, 1, core::DateRange(core::Date(2026, 11, 10), core::Date(2026, 11, 15)), 50.0));
        pRepo.add(std::make_unique<models::PaymentRecord>(500, 100, 10, 50.0, core::Date(2026, 11, 10), core::PaymentType::Payment, core::PaymentMethod::Cash, "V1"));
        mRepo.add(std::make_unique<models::MaintenanceRecord>(300, 1, "V1 maintenance", core::Date(2026, 11, 1), 100.0));

        storage.saveAll(vRepo, uRepo, rRepo, pRepo, mRepo);

        // Now prepare updated in-memory data (v2)
        vRepo.add(std::make_unique<models::Bike>(2, "Honda", "CBR", "XYZ-999", 30.0, 150, true));
        uRepo.add(std::make_unique<models::Admin>(20, "admin", "Admin User", "adm@test.com", 5, "HQ"));
        rRepo.add(std::make_unique<models::RentalRecord>(101, 10, 2, core::DateRange(core::Date(2026, 11, 20), core::Date(2026, 11, 25)), 30.0));

        // Lock payments.csv with an open handle to force failure on saving the 4th dataset
        std::filesystem::path paymentsPath = SANDBOX_DIR / "payments.csv";
        {
            std::ofstream lock(paymentsPath, std::ios::in | std::ios::out);
            VERIFY(lock.is_open());

            // Attempt saveAll: should throw on payments.csv
            bool caught = false;
            try {
                storage.saveAll(vRepo, uRepo, rRepo, pRepo, mRepo);
            } catch (const core::StorageException& ex) {
                caught = true;
                std::string msg = ex.what();
                VERIFY(msg.find("payments.csv") != std::string::npos);
            }
            VERIFY(caught);
        }

        // Verify multi-file save reality:
        // 1. vehicles.csv, users.csv, rentals.csv were already saved (now contain v2)
        // 2. payments.csv write failed and was preserved at v1
        // 3. maintenance.csv was never reached and preserved at v1
        Repository<models::Vehicle> checkV;
        Repository<models::User> checkU;
        Repository<models::RentalRecord> checkR;
        Repository<models::PaymentRecord> checkP;
        Repository<models::MaintenanceRecord> checkM;

        auto report = storage.loadAll(checkV, checkU, checkR, checkP, checkM);

        VERIFY(checkV.size() == 2); // Vehicles updated to v2
        VERIFY(checkU.size() == 2); // Users updated to v2
        VERIFY(checkR.size() == 2); // Rentals updated to v2
        VERIFY(checkP.size() == 1); // Payments preserved at v1
        VERIFY(checkM.size() == 1); // Maintenance preserved at v1

        cleanSandbox();
    } PASS;
}

// ============================================================================
// 9. LEFTOVER BACKUP SCENARIO TEST
// ============================================================================

void testLeftoverBackupScenario() {
    TEST("Safe write proceeds when both target and .bak exist from a prior crash") {
        cleanSandbox();
        std::filesystem::create_directories(SANDBOX_DIR);
        std::filesystem::path target = SANDBOX_DIR / "data.csv";
        std::filesystem::path bak = SANDBOX_DIR / "data.csv.bak";

        // Original target content (newer)
        FileStorage::safeWriteFile(target, {"ORIGINAL"});
        // Simulate leftover backup from a crash (older version)
        {
            std::ofstream ofs(bak);
            ofs << "OLD BACKUP" << "\n";
        }
        VERIFY(std::filesystem::exists(target));
        VERIFY(std::filesystem::exists(bak));

        // Now perform another safe write – should succeed, remove old .bak, and write new content
        FileStorage::safeWriteFile(target, {"NEW CONTENT"});
        VERIFY(std::filesystem::exists(target));
        VERIFY(!std::filesystem::exists(bak)); // old backup cleared
        std::ifstream ifs(target);
        std::string line;
        std::getline(ifs, line);
        VERIFY(line == "NEW CONTENT");

        cleanSandbox();
    } PASS;
}

void testActualStartDateSaveLoadPreservation() {
    TEST("FileStorage saves 10-column header and preserves actualStartDate round-trip") {
        cleanSandbox();
        FileStorage storage(SANDBOX_DIR);

        Repository<models::RentalRecord> rRepo;
        core::DateRange range(core::Date(2026, 11, 10), core::Date(2026, 11, 15));

        // 1. Confirmed rental (actualStartDate is nullopt)
        auto r1 = std::make_unique<models::RentalRecord>(101, 10, 1, range, 3000.0);

        // 2. Active rental with actualStartDate set
        auto r2 = std::make_unique<models::RentalRecord>(102, 10, 1, range, 3000.0);
        r2->markActive(core::Date(2026, 11, 12));

        // 3. Completed rental with actualStartDate set
        auto r3 = std::make_unique<models::RentalRecord>(103, 10, 1, range, 3000.0);
        r3->markActive(core::Date(2026, 11, 11));
        r3->completeRental(core::Date(2026, 11, 16), 4200.0);

        rRepo.add(r1);
        rRepo.add(r2);
        rRepo.add(r3);

        storage.saveRentals(rRepo);

        // Inspect header in rentals.csv
        std::ifstream ifs(SANDBOX_DIR / "rentals.csv");
        VERIFY(ifs.is_open());
        std::string headerLine;
        std::getline(ifs, headerLine);
        VERIFY(headerLine == "rentalId,customerId,vehicleId,startDate,scheduledEndDate,actualReturnDate,estimatedCost,finalCost,status,actualStartDate");
        ifs.close();

        // Load back
        Repository<models::RentalRecord> loadedR;
        std::vector<std::string> warnings;
        std::size_t loaded = storage.loadRentals(loadedR, warnings);
        VERIFY(loaded == 3);
        VERIFY(warnings.empty());

        auto* rec1 = loadedR.getById(101);
        VERIFY(rec1 != nullptr);
        VERIFY(rec1->getStatus() == core::RentalStatus::Confirmed);
        VERIFY(!rec1->getActualStartDate().has_value());

        auto* rec2 = loadedR.getById(102);
        VERIFY(rec2 != nullptr);
        VERIFY(rec2->getStatus() == core::RentalStatus::Active);
        VERIFY(rec2->getActualStartDate().has_value() && *rec2->getActualStartDate() == core::Date(2026, 11, 12));

        auto* rec3 = loadedR.getById(103);
        VERIFY(rec3 != nullptr);
        VERIFY(rec3->getStatus() == core::RentalStatus::Completed);
        VERIFY(rec3->getActualStartDate().has_value() && *rec3->getActualStartDate() == core::Date(2026, 11, 11));
        VERIFY(rec3->getActualReturnDate().has_value() && *rec3->getActualReturnDate() == core::Date(2026, 11, 16));
        VERIFY(rec3->getFinalCost().has_value() && *rec3->getFinalCost() == 4200.0);

        cleanSandbox();
    } PASS;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "\n===== 1. MISSING FILES & CONFIGURABLE PATH TESTS =====\n";
    testMissingFilesHandling();
    
    std::cout << "\n===== 2. FULL SAVE & LOAD ROUND-TRIP TESTS =====\n";
    testFullSaveAndLoadRoundTrip();
    
    std::cout << "\n===== 3. DETERMINISTIC SORTING TESTS =====\n";
    testDeterministicSorting();
    
    std::cout << "\n===== 4. SYNTAX ERRORS & CORRUPTED ROWS TESTS =====\n";
    testSyntaxWarningsAndCorruptedRows();
    
    std::cout << "\n===== 5. REFERENTIAL INTEGRITY & QUARANTINE TESTS =====\n";
    testReferentialIntegrityQuarantine();
    
    std::cout << "\n===== 6. SAFE FILE REPLACEMENT & PRESERVATION TESTS =====\n";
    testSafeFileReplacementAndPreservation();
    
    std::cout << "\n===== 7. TRANSACTIONAL LOADING & CRASH RECOVERY TESTS =====\n";
    testTransactionalLoadingPreservesOperationalRepositories();
    testCrashRecoveryScenarios();
    
    std::cout << "\n===== 8. MULTI-FILE SAVE LIMITATION VERIFICATION =====\n";
    testMultiFileSaveLimitations();

    std::cout << "\n===== 9. ACTUAL START DATE PERSISTENCE TESTS =====\n";
    testActualStartDateSaveLoadPreservation();
    
    std::cout << "\n===== RESULTS =====\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cerr << "*** SOME FILE STORAGE TESTS FAILED ***\n";
        return 1;
    }
    std::cout << "All FileStorage tests passed successfully.\n\n";
    return 0;
}

