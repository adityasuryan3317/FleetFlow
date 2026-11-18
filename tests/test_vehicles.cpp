#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Bike.hpp"
#include "fleetflow/models/Truck.hpp"
#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
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

void testCarConstructionAndGetters() {
    std::cout << "[RUNNING] testCarConstructionAndGetters..." << std::endl;

    Car sedan(1, "Honda", "Civic", "KA-01-AB-1234", 50.0, 5, "Petrol", true);
    TEST_ASSERT(sedan.getVehicleId() == 1, "Car ID is 1");
    TEST_ASSERT(sedan.getBrand() == "Honda", "Brand is Honda");
    TEST_ASSERT(sedan.getModel() == "Civic", "Model is Civic");
    TEST_ASSERT(sedan.getLicensePlate() == "KA-01-AB-1234", "License plate is KA-01-AB-1234");
    TEST_ASSERT(sedan.getBaseDailyRate() == 50.0, "Base rate is 50.0");
    TEST_ASSERT(sedan.getStatus() == VehicleStatus::Available, "Initial status is Available");
    TEST_ASSERT(sedan.getSeatingCapacity() == 5, "Seats is 5");
    TEST_ASSERT(sedan.getFuelType() == "Petrol", "Fuel is Petrol");
    TEST_ASSERT(sedan.getIsAutomatic() == true, "IsAutomatic is true");
    TEST_ASSERT(sedan.getVehicleType() == "Car", "Vehicle type is Car");

    // Status mutation
    sedan.setStatus(VehicleStatus::UnderMaintenance);
    TEST_ASSERT(sedan.getStatus() == VehicleStatus::UnderMaintenance, "Status updated to UnderMaintenance");
}

void testBikeConstructionAndGetters() {
    std::cout << "[RUNNING] testBikeConstructionAndGetters..." << std::endl;

    Bike cruiser(2, "Royal Enfield", "Meteor 350", "DL-04-XY-9999", 35.0, 349, true);
    TEST_ASSERT(cruiser.getVehicleId() == 2, "Bike ID is 2");
    TEST_ASSERT(cruiser.getBrand() == "Royal Enfield", "Brand is Royal Enfield");
    TEST_ASSERT(cruiser.getModel() == "Meteor 350", "Model is Meteor 350");
    TEST_ASSERT(cruiser.getEngineCC() == 349, "Engine displacement is 349 cc");
    TEST_ASSERT(cruiser.getHasGear() == true, "HasGear is true");
    TEST_ASSERT(cruiser.getVehicleType() == "Bike", "Vehicle type is Bike");
}

void testTruckConstructionAndGetters() {
    std::cout << "[RUNNING] testTruckConstructionAndGetters..." << std::endl;

    Truck hauler(3, "Volvo", "FH16", "MH-12-TR-7777", 120.0, 15.5, 4);
    TEST_ASSERT(hauler.getVehicleId() == 3, "Truck ID is 3");
    TEST_ASSERT(hauler.getBrand() == "Volvo", "Brand is Volvo");
    TEST_ASSERT(hauler.getModel() == "FH16", "Model is FH16");
    TEST_ASSERT(hauler.getCargoCapacityTons() == 15.5, "Cargo capacity is 15.5 tons");
    TEST_ASSERT(hauler.getNumberOfAxles() == 4, "Axle count is 4");
    TEST_ASSERT(hauler.getVehicleType() == "Truck", "Vehicle type is Truck");
}

void testInvalidConstructorArguments() {
    std::cout << "[RUNNING] testInvalidConstructorArguments..." << std::endl;

    // 1. Negative or zero ID
    bool threw = false;
    try {
        Car badId(0, "Toyota", "Corolla", "ABC-123", 40.0, 5, "Petrol", false);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "ID <= 0 throws ValidationException");

    // 2. Empty brand
    threw = false;
    try {
        Car emptyBrand(1, "   ", "Corolla", "ABC-123", 40.0, 5, "Petrol", false);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank brand throws ValidationException");

    // 3. Empty model
    threw = false;
    try {
        Car emptyModel(1, "Toyota", "", "ABC-123", 40.0, 5, "Petrol", false);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Empty model throws ValidationException");

    // 4. Empty plate
    threw = false;
    try {
        Car emptyPlate(1, "Toyota", "Corolla", " \t ", 40.0, 5, "Petrol", false);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank license plate throws ValidationException");

    // 5. Non-positive base rate
    threw = false;
    try {
        Car badRate(1, "Toyota", "Corolla", "ABC-123", -10.0, 5, "Petrol", false);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Negative base daily rate throws ValidationException");

    // 6. Car seating capacity out of range
    threw = false;
    try {
        Car badSeats(1, "Toyota", "Corolla", "ABC-123", 40.0, 0, "Petrol", false);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Car seats < 1 throws ValidationException");

    threw = false;
    try {
        Car badSeats(1, "Toyota", "HiAce", "ABC-123", 40.0, 15, "Diesel", false);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Car seats > 9 throws ValidationException");

    // 7. Bike engine CC <= 0
    threw = false;
    try {
        Bike badCC(2, "Yamaha", "R15", "XYZ-789", 25.0, 0, true);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Bike engine CC <= 0 throws ValidationException");

    // 8. Truck cargo capacity <= 0
    threw = false;
    try {
        Truck badCargo(3, "Tata", "Prima", "TRK-001", 100.0, -1.0, 2);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Truck cargo capacity <= 0 throws ValidationException");

    // 9. Truck axles < 2
    threw = false;
    try {
        Truck badAxles(3, "Tata", "Prima", "TRK-001", 100.0, 5.0, 1);
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Truck axles < 2 throws ValidationException");
}

void testPricingFormulas() {
    std::cout << "[RUNNING] testPricingFormulas..." << std::endl;

    // --- CAR PRICING ---
    // Formula: baseDailyRate + (isAutomatic ? 15.0 : 0.0) + (seatingCapacity > 5 ? 20.0 : 0.0)

    // Manual Sedan (5 seats): base = 50.0 -> rate = 50.0
    Car manualSedan(1, "VW", "Virtus", "KA-01-1111", 50.0, 5, "Petrol", false);
    TEST_ASSERT_NEAR(manualSedan.calculateDailyRate(), 50.0, 0.001, "Manual sedan rate is base rate (50.0)");

    // Automatic Sedan (5 seats): base = 50.0 + 15.0 = 65.0
    Car autoSedan(2, "VW", "Virtus GT", "KA-01-2222", 50.0, 5, "Petrol", true);
    TEST_ASSERT_NEAR(autoSedan.calculateDailyRate(), 65.0, 0.001, "Automatic sedan rate is base + 15.0 (65.0)");

    // Automatic 7-Seater SUV: base = 70.0 + 15.0 (auto) + 20.0 (seats > 5) = 105.0
    Car autoSuv(3, "Toyota", "Innova", "KA-01-3333", 70.0, 7, "Diesel", true);
    TEST_ASSERT_NEAR(autoSuv.calculateDailyRate(), 105.0, 0.001, "Automatic 7-seater SUV rate is base + 15 + 20 (105.0)");

    // --- BIKE PRICING ---
    // Formula: baseDailyRate + (engineCC > 350 ? 15.0 : 0.0) - (!hasGear ? 5.0 : 0.0)

    // Standard geared commuter (150 cc): base = 30.0 -> rate = 30.0
    Bike commuter(4, "Honda", "Unicorn", "KA-02-4444", 30.0, 150, true);
    TEST_ASSERT_NEAR(commuter.calculateDailyRate(), 30.0, 0.001, "Geared commuter bike rate is base rate (30.0)");

    // Gearless scooter (110 cc): base = 30.0 - 5.0 = 25.0 (verifies discount, not surcharge)
    Bike scooter(5, "Honda", "Activa", "KA-02-5555", 30.0, 110, false);
    TEST_ASSERT_NEAR(scooter.calculateDailyRate(), 25.0, 0.001, "Gearless scooter rate has 5.0 discount (25.0)");

    // Explicit comparison: geared vs gearless with identical specs verifies 5.0 discount
    Bike geared125(20, "Honda", "SP125", "KA-01-2020", 30.0, 125, true);
    Bike gearless125(21, "Honda", "Activa 125", "KA-01-2021", 30.0, 125, false);
    TEST_ASSERT_NEAR(gearless125.calculateDailyRate(), geared125.calculateDailyRate() - 5.0, 0.001,
                     "Gearless bike receives exactly 5.0 discount compared to equivalent geared bike");

    // Minimum daily rate floor verification: base 6.0 - 5.0 discount = 1.0 -> clamped to 5.0
    Bike lowRateScooter(22, "TVS", "Moped", "KA-01-2022", 6.0, 99, false);
    TEST_ASSERT_NEAR(lowRateScooter.calculateDailyRate(), 5.0, 0.001,
                     "Low base rate gearless bike is clamped to minimum daily rate of 5.0");

    // Superbike (650 cc, geared): base = 60.0 + 15.0 = 75.0
    Bike superbike(6, "Kawasaki", "Ninja 650", "KA-02-6666", 60.0, 650, true);
    TEST_ASSERT_NEAR(superbike.calculateDailyRate(), 75.0, 0.001, "High-displacement superbike rate has 15.0 surcharge (75.0)");

    // --- TRUCK PRICING ---
    // Formula: baseDailyRate + (cargoCapacityTons * 10.0) + ((numberOfAxles - 2) * 25.0)

    // Light 2-axle commercial truck (3.0 tons): base = 80.0 + (3.0 * 10.0) = 110.0
    Truck lightTruck(7, "Tata", "407", "KA-03-7777", 80.0, 3.0, 2);
    TEST_ASSERT_NEAR(lightTruck.calculateDailyRate(), 110.0, 0.001, "Light truck rate = 80 + 3*10 (110.0)");

    // Heavy 4-axle commercial hauler (12.0 tons): base = 150.0 + (12.0 * 10.0) + (2 extra axles * 25.0) = 320.0
    Truck heavyTruck(8, "BharatBenz", "3523R", "KA-03-8888", 150.0, 12.0, 4);
    TEST_ASSERT_NEAR(heavyTruck.calculateDailyRate(), 320.0, 0.001, "Heavy truck rate = 150 + 120 + 50 (320.0)");
}

void testRuntimePolymorphism() {
    std::cout << "[RUNNING] testRuntimePolymorphism..." << std::endl;

    // Polymorphic container holding unique_ptr<Vehicle>
    std::vector<std::unique_ptr<Vehicle>> fleet;

    fleet.push_back(std::make_unique<Car>(101, "Hyundai", "i20", "KA-05-C-101", 45.0, 5, "Petrol", true));
    fleet.push_back(std::make_unique<Bike>(102, "TVS", "Apache RTR", "KA-05-B-102", 28.0, 160, true));
    fleet.push_back(std::make_unique<Truck>(103, "Ashok Leyland", "Ecomet", "KA-05-T-103", 95.0, 7.5, 3));

    // Dynamic dispatch of calculateDailyRate()
    // Car: 45.0 + 15.0 = 60.0
    TEST_ASSERT_NEAR(fleet[0]->calculateDailyRate(), 60.0, 0.001, "Polymorphic dispatch for Car rate (60.0)");
    TEST_ASSERT(fleet[0]->getVehicleType() == "Car", "Polymorphic type is Car");

    // Bike: 28.0
    TEST_ASSERT_NEAR(fleet[1]->calculateDailyRate(), 28.0, 0.001, "Polymorphic dispatch for Bike rate (28.0)");
    TEST_ASSERT(fleet[1]->getVehicleType() == "Bike", "Polymorphic type is Bike");

    // Truck: 95.0 + (7.5 * 10.0) + (1 extra axle * 25.0) = 95 + 75 + 25 = 195.0
    TEST_ASSERT_NEAR(fleet[2]->calculateDailyRate(), 195.0, 0.001, "Polymorphic dispatch for Truck rate (195.0)");
    TEST_ASSERT(fleet[2]->getVehicleType() == "Truck", "Polymorphic type is Truck");

    // Stream-based displayDetails verification
    std::ostringstream oss;
    fleet[0]->displayDetails(oss);
    std::string output = oss.str();
    TEST_ASSERT(output.find("[CAR] Hyundai i20") != std::string::npos, "Formatted output contains Car banner");
    TEST_ASSERT(output.find("Transmission     : Automatic") != std::string::npos, "Formatted output contains transmission");
    TEST_ASSERT(output.find("Effective Rate   : ₹60.00/day") != std::string::npos, "Formatted output contains INR effective rate");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  FleetFlow Test Suite: Vehicle Hierarchy" << std::endl;
    std::cout << "========================================" << std::endl;

    testCarConstructionAndGetters();
    testBikeConstructionAndGetters();
    testTruckConstructionAndGetters();
    testInvalidConstructorArguments();
    testPricingFormulas();
    testRuntimePolymorphism();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (g_testsFailed == 0) ? 0 : 1;
}

