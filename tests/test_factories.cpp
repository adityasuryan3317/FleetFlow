/**
 * @file test_factories.cpp
 * @brief Comprehensive tests for VehicleFactory and UserFactory.
 *
 * Covers: valid construction, case-insensitive discriminators, exact token counts,
 * strict numeric parsing (whitespace rejection), boolean parsing, VehicleStatus
 * conversion, domain-exception translation, and Bike-specific engineCC validation.
 */

#include "fleetflow/factories/VehicleFactory.hpp"
#include "fleetflow/factories/UserFactory.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Bike.hpp"
#include "fleetflow/models/Truck.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/Employee.hpp"
#include "fleetflow/models/Admin.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/core/CommonTypes.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

using namespace fleetflow;

// ============================================================================
// Helper
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
// VEHICLE FACTORY TESTS
// ============================================================================

void testCreateCarValid() {
    TEST("Create valid Car") {
        std::vector<std::string> tokens = {
            "1", "Toyota", "Camry", "ABC-1234", "50.0", "Available", "5", "Petrol", "true"
        };
        auto vehicle = factories::VehicleFactory::createFromTokens("CAR", tokens);
        VERIFY(vehicle != nullptr);
        VERIFY(vehicle->getVehicleType() == "Car");
        VERIFY(vehicle->getVehicleId() == 1);
        VERIFY(vehicle->getBrand() == "Toyota");
        VERIFY(vehicle->getModel() == "Camry");
        VERIFY(vehicle->getLicensePlate() == "ABC-1234");
        VERIFY(vehicle->getBaseDailyRate() == 50.0);
        VERIFY(vehicle->getStatus() == core::VehicleStatus::Available);

        auto* car = dynamic_cast<models::Car*>(vehicle.get());
        VERIFY(car != nullptr);
        VERIFY(car->getSeatingCapacity() == 5);
        VERIFY(car->getFuelType() == "Petrol");
        VERIFY(car->getIsAutomatic() == true);
    } PASS;
}

void testCreateBikeValid() {
    TEST("Create valid Bike") {
        std::vector<std::string> tokens = {
            "2", "Honda", "CBR", "XYZ-5678", "30.0", "Available", "600", "true"
        };
        auto vehicle = factories::VehicleFactory::createFromTokens("BIKE", tokens);
        VERIFY(vehicle != nullptr);
        VERIFY(vehicle->getVehicleType() == "Bike");
        VERIFY(vehicle->getVehicleId() == 2);

        auto* bike = dynamic_cast<models::Bike*>(vehicle.get());
        VERIFY(bike != nullptr);
        VERIFY(bike->getEngineCC() == 600);
        VERIFY(bike->getHasGear() == true);
    } PASS;
}

void testCreateTruckValid() {
    TEST("Create valid Truck") {
        std::vector<std::string> tokens = {
            "3", "Volvo", "FH16", "TRK-9999", "100.0", "Available", "12.5", "4"
        };
        auto vehicle = factories::VehicleFactory::createFromTokens("TRUCK", tokens);
        VERIFY(vehicle != nullptr);
        VERIFY(vehicle->getVehicleType() == "Truck");
        VERIFY(vehicle->getVehicleId() == 3);

        auto* truck = dynamic_cast<models::Truck*>(vehicle.get());
        VERIFY(truck != nullptr);
        VERIFY(truck->getCargoCapacityTons() == 12.5);
        VERIFY(truck->getNumberOfAxles() == 4);
    } PASS;
}

void testCaseInsensitiveVehicleType() {
    TEST("Case-insensitive vehicle type") {
        std::vector<std::string> bikeTokens = {
            "10", "Yamaha", "R15", "YMH-0001", "25.0", "Available", "150", "true"
        };
        // Lowercase
        auto v1 = factories::VehicleFactory::createFromTokens("bike", bikeTokens);
        VERIFY(v1 != nullptr && v1->getVehicleType() == "Bike");

        // Mixed case
        bikeTokens[0] = "11";
        auto v2 = factories::VehicleFactory::createFromTokens("Bike", bikeTokens);
        VERIFY(v2 != nullptr && v2->getVehicleType() == "Bike");

        // Uppercase
        bikeTokens[0] = "12";
        auto v3 = factories::VehicleFactory::createFromTokens("BIKE", bikeTokens);
        VERIFY(v3 != nullptr && v3->getVehicleType() == "Bike");
    } PASS;
}

void testCaseInsensitiveVehicleStatus() {
    TEST("Case-insensitive VehicleStatus conversion") {
        // Lowercase "available"
        std::vector<std::string> tokens1 = {
            "20", "Ford", "Ranger", "FRD-0001", "60.0", "available", "5", "Diesel", "false"
        };
        auto v1 = factories::VehicleFactory::createFromTokens("CAR", tokens1);
        VERIFY(v1->getStatus() == core::VehicleStatus::Available);

        // Mixed case "UnderMaintenance"
        std::vector<std::string> tokens2 = {
            "21", "Ford", "Ranger", "FRD-0002", "60.0", "UnderMaintenance", "5", "Diesel", "false"
        };
        auto v2 = factories::VehicleFactory::createFromTokens("CAR", tokens2);
        VERIFY(v2->getStatus() == core::VehicleStatus::UnderMaintenance);

        // Uppercase "UNDERMAINTENANCE"
        std::vector<std::string> tokens3 = {
            "22", "Ford", "Ranger", "FRD-0003", "60.0", "UNDERMAINTENANCE", "5", "Diesel", "false"
        };
        auto v3 = factories::VehicleFactory::createFromTokens("CAR", tokens3);
        VERIFY(v3->getStatus() == core::VehicleStatus::UnderMaintenance);

        // "retired"
        std::vector<std::string> tokens4 = {
            "23", "Ford", "Ranger", "FRD-0004", "60.0", "retired", "5", "Diesel", "false"
        };
        auto v4 = factories::VehicleFactory::createFromTokens("CAR", tokens4);
        VERIFY(v4->getStatus() == core::VehicleStatus::Retired);
    } PASS;
}

void testUnknownVehicleType() {
    TEST("Unknown vehicle type throws StorageException") {
        std::vector<std::string> tokens = {"1", "X", "Y", "Z", "10.0", "Available", "2", "Gas", "true"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("SUBMARINE", tokens),
            core::StorageException
        );
    } PASS;
}

void testInvalidVehicleStatus() {
    TEST("Invalid VehicleStatus throws StorageException") {
        std::vector<std::string> tokens = {
            "1", "Toyota", "Camry", "ABC-0001", "50.0", "Rented", "5", "Petrol", "true"
        };
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", tokens),
            core::StorageException
        );
    } PASS;
}

void testWrongTokenCountVehicle() {
    TEST("Wrong token count for Vehicle throws StorageException") {
        // Car expects 9, give 7
        std::vector<std::string> tokens = {"1", "A", "B", "C", "10.0", "Available", "5"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", tokens),
            core::StorageException
        );
        // Bike expects 8, give 9
        std::vector<std::string> tokens2 = {"1", "A", "B", "C", "10.0", "Available", "150", "true", "extra"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("BIKE", tokens2),
            core::StorageException
        );
    } PASS;
}

void testInvalidNumericVehicle() {
    TEST("Invalid numeric tokens throw StorageException") {
        // Non-numeric id
        std::vector<std::string> tokens = {"abc", "A", "B", "C", "10.0", "Available", "5", "Gas", "true"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", tokens),
            core::StorageException
        );
        // Non-numeric baseDailyRate
        std::vector<std::string> tokens2 = {"1", "A", "B", "C", "xyz", "Available", "5", "Gas", "true"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", tokens2),
            core::StorageException
        );
    } PASS;
}

void testWhitespaceNumericVehicle() {
    TEST("Whitespace in numeric tokens throws StorageException") {
        // Leading whitespace in id: " 1"
        std::vector<std::string> t1 = {" 1", "A", "B", "C", "10.0", "Available", "5", "Gas", "true"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", t1),
            core::StorageException
        );
        // Trailing whitespace in id: "1 "
        std::vector<std::string> t2 = {"1 ", "A", "B", "C", "10.0", "Available", "5", "Gas", "true"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", t2),
            core::StorageException
        );
        // Both: " 1 "
        std::vector<std::string> t3 = {" 1 ", "A", "B", "C", "10.0", "Available", "5", "Gas", "true"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", t3),
            core::StorageException
        );
        // Whitespace in double: " 10.5"
        std::vector<std::string> t4 = {"1", "A", "B", "C", " 10.5", "Available", "5", "Gas", "true"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", t4),
            core::StorageException
        );
    } PASS;
}

void testInvalidBooleanVehicle() {
    TEST("Invalid boolean token throws StorageException") {
        std::vector<std::string> tokens = {"1", "A", "B", "C", "10.0", "Available", "5", "Gas", "maybe"};
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", tokens),
            core::StorageException
        );
    } PASS;
}

void testBikeInvalidEngineCC() {
    TEST("Bike with zero engineCC throws StorageException (domain validation)") {
        std::vector<std::string> tokens = {
            "50", "Honda", "Activa", "ACT-0001", "15.0", "Available", "0", "false"
        };
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("BIKE", tokens),
            core::StorageException
        );
    } PASS;

    TEST("Bike with negative engineCC throws StorageException (domain validation)") {
        std::vector<std::string> tokens = {
            "51", "Honda", "Activa", "ACT-0002", "15.0", "Available", "-100", "false"
        };
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("BIKE", tokens),
            core::StorageException
        );
    } PASS;
}

void testDomainValidationWrapping() {
    TEST("Domain ValidationException wrapped as StorageException") {
        // Vehicle with zero baseDailyRate → Vehicle constructor throws ValidationException
        std::vector<std::string> tokens = {
            "99", "Brand", "Model", "PLT-0001", "0.0", "Available", "5", "Gas", "true"
        };
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", tokens),
            core::StorageException
        );
        // Car with seating > 9 → Car constructor throws ValidationException
        std::vector<std::string> tokens2 = {
            "100", "Brand", "Model", "PLT-0002", "50.0", "Available", "15", "Gas", "true"
        };
        EXPECT_THROW(
            factories::VehicleFactory::createFromTokens("CAR", tokens2),
            core::StorageException
        );
    } PASS;
}

// ============================================================================
// USER FACTORY TESTS
// ============================================================================

void testCreateCustomerValid() {
    TEST("Create valid Customer") {
        std::vector<std::string> tokens = {
            "1", "john_doe", "John Doe", "john@example.com", "DL-12345", "+1234567890"
        };
        auto user = factories::UserFactory::createFromTokens("CUSTOMER", tokens);
        VERIFY(user != nullptr);
        VERIFY(user->getRoleName() == "Customer");
        VERIFY(user->getUserId() == 1);
        VERIFY(user->getUsername() == "john_doe");
        VERIFY(user->getFullName() == "John Doe");
        VERIFY(user->getEmail() == "john@example.com");

        auto* cust = dynamic_cast<models::Customer*>(user.get());
        VERIFY(cust != nullptr);
        VERIFY(cust->getDrivingLicense() == "DL-12345");
        VERIFY(cust->getPhoneNumber() == "+1234567890");
    } PASS;
}

void testCreateEmployeeValid() {
    TEST("Create valid Employee") {
        std::vector<std::string> tokens = {
            "2", "jane_emp", "Jane Smith", "jane@fleet.com", "EMP-001", "Operations"
        };
        auto user = factories::UserFactory::createFromTokens("EMPLOYEE", tokens);
        VERIFY(user != nullptr);
        VERIFY(user->getRoleName() == "Employee");

        auto* emp = dynamic_cast<models::Employee*>(user.get());
        VERIFY(emp != nullptr);
        VERIFY(emp->getEmployeeCode() == "EMP-001");
        VERIFY(emp->getDepartment() == "Operations");
    } PASS;
}

void testCreateAdminValid() {
    TEST("Create valid Admin") {
        std::vector<std::string> tokens = {
            "3", "admin_x", "Admin User", "admin@fleet.com", "5", "Headquarters"
        };
        auto user = factories::UserFactory::createFromTokens("ADMIN", tokens);
        VERIFY(user != nullptr);
        VERIFY(user->getRoleName() == "Admin");

        auto* admin = dynamic_cast<models::Admin*>(user.get());
        VERIFY(admin != nullptr);
        VERIFY(admin->getSecurityClearance() == 5);
        VERIFY(admin->getDesignatedOffice() == "Headquarters");
    } PASS;
}

void testCaseInsensitiveUserRole() {
    TEST("Case-insensitive user role") {
        std::vector<std::string> tokens = {
            "30", "user_a", "User A", "a@test.com", "DL-999", "+0000000000"
        };
        auto v1 = factories::UserFactory::createFromTokens("customer", tokens);
        VERIFY(v1 != nullptr && v1->getRoleName() == "Customer");

        tokens[0] = "31";
        auto v2 = factories::UserFactory::createFromTokens("Customer", tokens);
        VERIFY(v2 != nullptr && v2->getRoleName() == "Customer");

        tokens[0] = "32";
        auto v3 = factories::UserFactory::createFromTokens("CUSTOMER", tokens);
        VERIFY(v3 != nullptr && v3->getRoleName() == "Customer");
    } PASS;
}

void testUnknownUserRole() {
    TEST("Unknown user role throws StorageException") {
        std::vector<std::string> tokens = {"1", "u", "N", "x@y.com", "a", "b"};
        EXPECT_THROW(
            factories::UserFactory::createFromTokens("SUPERUSER", tokens),
            core::StorageException
        );
    } PASS;
}

void testWrongTokenCountUser() {
    TEST("Wrong token count for User throws StorageException") {
        // Customer expects 6, give 4
        std::vector<std::string> tokens = {"1", "u", "N", "x@y.com"};
        EXPECT_THROW(
            factories::UserFactory::createFromTokens("CUSTOMER", tokens),
            core::StorageException
        );
    } PASS;
}

void testWhitespaceNumericUser() {
    TEST("Whitespace in User numeric tokens throws StorageException") {
        // Leading whitespace in id
        std::vector<std::string> t1 = {" 1", "u", "Name", "x@y.com", "DL", "12345"};
        EXPECT_THROW(
            factories::UserFactory::createFromTokens("CUSTOMER", t1),
            core::StorageException
        );
        // Trailing whitespace in Admin clearance
        std::vector<std::string> t2 = {"1", "u", "Name", "x@y.com", "5 ", "Office"};
        EXPECT_THROW(
            factories::UserFactory::createFromTokens("ADMIN", t2),
            core::StorageException
        );
    } PASS;
}

void testUserDomainValidation() {
    TEST("User domain validation errors wrapped as StorageException") {
        // Empty driving license → Customer throws ValidationException
        std::vector<std::string> t1 = {"40", "u", "Name", "x@y.com", "", "12345"};
        EXPECT_THROW(
            factories::UserFactory::createFromTokens("CUSTOMER", t1),
            core::StorageException
        );
        // Empty employee code → Employee throws ValidationException
        std::vector<std::string> t2 = {"41", "u", "Name", "x@y.com", "", "Dept"};
        EXPECT_THROW(
            factories::UserFactory::createFromTokens("EMPLOYEE", t2),
            core::StorageException
        );
        // Admin clearance < 1 → Admin throws ValidationException
        std::vector<std::string> t3 = {"42", "u", "Name", "x@y.com", "0", "Office"};
        EXPECT_THROW(
            factories::UserFactory::createFromTokens("ADMIN", t3),
            core::StorageException
        );
        // Invalid email → User constructor throws ValidationException
        std::vector<std::string> t4 = {"43", "u", "Name", "bademail", "DL", "12345"};
        EXPECT_THROW(
            factories::UserFactory::createFromTokens("CUSTOMER", t4),
            core::StorageException
        );
    } PASS;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "\n===== VEHICLE FACTORY TESTS =====\n";
    testCreateCarValid();
    testCreateBikeValid();
    testCreateTruckValid();
    testCaseInsensitiveVehicleType();
    testCaseInsensitiveVehicleStatus();
    testUnknownVehicleType();
    testInvalidVehicleStatus();
    testWrongTokenCountVehicle();
    testInvalidNumericVehicle();
    testWhitespaceNumericVehicle();
    testInvalidBooleanVehicle();
    testBikeInvalidEngineCC();
    testDomainValidationWrapping();

    std::cout << "\n===== USER FACTORY TESTS =====\n";
    testCreateCustomerValid();
    testCreateEmployeeValid();
    testCreateAdminValid();
    testCaseInsensitiveUserRole();
    testUnknownUserRole();
    testWrongTokenCountUser();
    testWhitespaceNumericUser();
    testUserDomainValidation();

    std::cout << "\n===== RESULTS =====\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";

    if (failed > 0) {
        std::cerr << "*** SOME FACTORY TESTS FAILED ***\n";
        return 1;
    }
    std::cout << "All factory tests passed.\n\n";
    return 0;
}
