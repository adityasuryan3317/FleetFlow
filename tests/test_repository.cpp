#include "fleetflow/storage/Repository.hpp"
#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Bike.hpp"
#include "fleetflow/models/Truck.hpp"
#include "fleetflow/models/User.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/Employee.hpp"
#include "fleetflow/models/Admin.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <iostream>
#include <memory>
#include <vector>
#include <string>

using namespace fleetflow::storage;
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

// Helper tracked struct for testing automatic RAII destruction
struct TrackedItem {
    static int s_destructCount;
    int id{0};
    TrackedItem(int i) : id(i) {}
    ~TrackedItem() { s_destructCount++; }
};
int TrackedItem::s_destructCount = 0;

namespace fleetflow::storage {
template <>
struct EntityTraits<TrackedItem> {
    static int getId(const TrackedItem& item) noexcept {
        return item.id;
    }
};
}

void testVehicleRepositoryOperations() {
    std::cout << "[RUNNING] testVehicleRepositoryOperations..." << std::endl;

    Repository<Vehicle> repo;
    TEST_ASSERT(repo.empty() == true, "New repository is empty");
    TEST_ASSERT(repo.size() == 0, "New repository size is 0");

    // 1. Insert a Car
    auto car = std::make_unique<Car>(1, "Honda", "City", "KA-01-1111", 60.0, 5, "Petrol", true);
    repo.add(std::move(car));
    TEST_ASSERT(repo.size() == 1, "Repo size is 1 after inserting Car");

    // 2. Insert a Bike
    auto bike = std::make_unique<Bike>(2, "Yamaha", "R15", "KA-01-2222", 30.0, 155, true);
    repo.add(std::move(bike));
    TEST_ASSERT(repo.size() == 2, "Repo size is 2 after inserting Bike");

    // 3. Insert a Truck
    auto truck = std::make_unique<Truck>(3, "Tata", "Prima", "KA-01-3333", 150.0, 10.0, 3);
    repo.add(std::move(truck));
    TEST_ASSERT(repo.size() == 3, "Repo size is 3 after inserting Truck");

    // 4. Retrieve objects by ID
    Vehicle* v1 = repo.getById(1);
    TEST_ASSERT(v1 != nullptr, "Vehicle #1 found");
    TEST_ASSERT(v1->getBrand() == "Honda", "Vehicle #1 brand is Honda");
    TEST_ASSERT(v1->getVehicleType() == "Car", "Vehicle #1 type is Car");

    Vehicle* v2 = repo.getById(2);
    TEST_ASSERT(v2 != nullptr, "Vehicle #2 found");
    TEST_ASSERT(v2->getVehicleType() == "Bike", "Vehicle #2 type is Bike");

    Vehicle* v3 = repo.getById(3);
    TEST_ASSERT(v3 != nullptr, "Vehicle #3 found");
    TEST_ASSERT(v3->getVehicleType() == "Truck", "Vehicle #3 type is Truck");

    // 5. Test nonexistent IDs
    TEST_ASSERT(repo.getById(999) == nullptr, "Nonexistent ID returns nullptr");
    TEST_ASSERT(repo.exists(999) == false, "exists(999) is false");
    TEST_ASSERT(repo.exists(1) == true, "exists(1) is true");

    // 6. Retrieve all vehicles
    std::vector<Vehicle*> all = repo.getAll();
    TEST_ASSERT(all.size() == 3, "getAll() returns 3 items");

    // Const repository test
    const Repository<Vehicle>& constRepo = repo;
    const Vehicle* cv = constRepo.getById(1);
    TEST_ASSERT(cv != nullptr && cv->getBrand() == "Honda", "Const getById() works");
    std::vector<const Vehicle*> constAll = constRepo.getAll();
    TEST_ASSERT(constAll.size() == 3, "Const getAll() works");

    // 11. Test remove()
    TEST_ASSERT(repo.remove(2) == true, "remove(2) returns true");
    TEST_ASSERT(repo.size() == 2, "Repo size is 2 after removal");
    TEST_ASSERT(repo.getById(2) == nullptr, "Vehicle #2 no longer exists");
    TEST_ASSERT(repo.remove(2) == false, "remove(2) again returns false");

    // 12. Test clear()
    repo.clear();
    TEST_ASSERT(repo.empty() == true, "Repo is empty after clear()");
    TEST_ASSERT(repo.size() == 0, "Repo size is 0 after clear()");
    TEST_ASSERT(repo.getById(1) == nullptr, "Vehicle #1 is cleared");
}

void testRejectionAndOwnershipGuarantees() {
    std::cout << "[RUNNING] testRejectionAndOwnershipGuarantees..." << std::endl;

    Repository<Vehicle> repo;
    auto initialCar = std::make_unique<Car>(10, "Toyota", "Yaris", "DL-01-100", 50.0, 5, "Petrol", true);

    // 9. Verify ownership transfer on success
    repo.add(initialCar);
    TEST_ASSERT(initialCar == nullptr, "Caller unique_ptr is reset to nullptr on successful insertion");
    TEST_ASSERT(repo.size() == 1, "Repo contains 1 vehicle");

    // 7. Reject duplicate IDs
    auto duplicateCar = std::make_unique<Car>(10, "Toyota", "Corolla", "DL-01-200", 55.0, 5, "Petrol", false);
    bool threwDuplicate = false;
    try {
        repo.add(duplicateCar);
    } catch (const DuplicateEntityException&) {
        threwDuplicate = true;
    }
    TEST_ASSERT(threwDuplicate, "Adding duplicate ID throws DuplicateEntityException");

    // 10. Verify ownership preservation after failed insertion!
    TEST_ASSERT(duplicateCar != nullptr, "Caller unique_ptr remains NON-NULL after duplicate rejection");
    TEST_ASSERT(duplicateCar->getModel() == "Corolla", "Object content is intact after failed insertion");
    TEST_ASSERT(repo.size() == 1, "Repo size unchanged after failed insertion");

    // 8. Reject nullptr
    std::unique_ptr<Vehicle> nullPtr;
    bool threwNull = false;
    try {
        repo.add(nullPtr);
    } catch (const ValidationException&) {
        threwNull = true;
    }
    TEST_ASSERT(threwNull, "Adding nullptr throws ValidationException");
}

void testUserRepositoryPolymorphism() {
    std::cout << "[RUNNING] testUserRepositoryPolymorphism..." << std::endl;

    // 14. Test User repository with Customer, Employee, Admin
    Repository<User> userRepo;

    userRepo.add(std::make_unique<Customer>(101, "cust_arjun", "Arjun Reddy", "arjun@gmail.com", "DL-9988", "9988776655"));
    userRepo.add(std::make_unique<Employee>(102, "emp_sneha", "Sneha Patel", "sneha@fleetflow.in", "EMP-007", "Desk Operations"));
    userRepo.add(std::make_unique<Admin>(103, "adm_vikram", "Vikram Malhotra", "vikram@fleetflow.in", 2, "HQ - Mumbai"));

    TEST_ASSERT(userRepo.size() == 3, "User repo size is 3");

    User* u1 = userRepo.getById(101);
    TEST_ASSERT(u1 != nullptr, "User #101 retrieved");
    TEST_ASSERT(u1->getRoleName() == "Customer", "User #101 polymorphic role is Customer");
    auto* cust = dynamic_cast<Customer*>(u1);
    TEST_ASSERT(cust != nullptr && cust->getDrivingLicense() == "DL-9988", "Customer downcast succeeds");

    User* u2 = userRepo.getById(102);
    TEST_ASSERT(u2 != nullptr && u2->getRoleName() == "Employee", "User #102 is Employee");
    auto* emp = dynamic_cast<Employee*>(u2);
    TEST_ASSERT(emp != nullptr && emp->getEmployeeCode() == "EMP-007", "Employee downcast succeeds");

    User* u3 = userRepo.getById(103);
    TEST_ASSERT(u3 != nullptr && u3->getRoleName() == "Admin", "User #103 is Admin");
    auto* adm = dynamic_cast<Admin*>(u3);
    TEST_ASSERT(adm != nullptr && adm->getSecurityClearance() == 2, "Admin downcast succeeds");
}

void testAutomaticDestructionAndMoveSemantics() {
    std::cout << "[RUNNING] testAutomaticDestructionAndMoveSemantics..." << std::endl;

    // 15. Test automatic object destruction when repository is destroyed
    TrackedItem::s_destructCount = 0;
    {
        Repository<TrackedItem> trackedRepo;
        trackedRepo.add(std::make_unique<TrackedItem>(1));
        trackedRepo.add(std::make_unique<TrackedItem>(2));
        trackedRepo.add(std::make_unique<TrackedItem>(3));
        TEST_ASSERT(trackedRepo.size() == 3, "Tracked repo has 3 items");
        TEST_ASSERT(TrackedItem::s_destructCount == 0, "No destructions while repo is alive");
    }
    // Sub-scope ended: repository destroyed
    TEST_ASSERT(TrackedItem::s_destructCount == 3, "All 3 TrackedItems destroyed automatically via unique_ptr RAII");

    // 16. Test repository move semantics
    Repository<Vehicle> r1;
    r1.add(std::make_unique<Car>(50, "Skoda", "Slavia", "DL-05-5555", 65.0, 5, "Petrol", true));
    r1.add(std::make_unique<Bike>(51, "KTM", "Duke 390", "DL-05-6666", 45.0, 373, true));
    TEST_ASSERT(r1.size() == 2, "r1 size is 2 before move");

    // Move constructor
    Repository<Vehicle> r2 = std::move(r1);
    TEST_ASSERT(r2.size() == 2, "r2 size is 2 after move construction");
    TEST_ASSERT(r2.getById(50) != nullptr, "Vehicle #50 accessible in moved-to r2");
    TEST_ASSERT(r2.getById(51) != nullptr, "Vehicle #51 accessible in moved-to r2");

    // Move assignment
    Repository<Vehicle> r3;
    r3 = std::move(r2);
    TEST_ASSERT(r3.size() == 2, "r3 size is 2 after move assignment");
    TEST_ASSERT(r3.getById(50)->getModel() == "Slavia", "Object data preserved in r3");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  FleetFlow Test Suite: Repository<T>   " << std::endl;
    std::cout << "========================================" << std::endl;

    testVehicleRepositoryOperations();
    testRejectionAndOwnershipGuarantees();
    testUserRepositoryPolymorphism();
    testAutomaticDestructionAndMoveSemantics();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (g_testsFailed == 0) ? 0 : 1;
}

