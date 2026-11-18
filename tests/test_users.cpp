#include "fleetflow/models/User.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/Employee.hpp"
#include "fleetflow/models/Admin.hpp"
#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <memory>

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

void testCustomerConstructionAndGetters() {
    std::cout << "[RUNNING] testCustomerConstructionAndGetters..." << std::endl;

    Customer cust(1, "rahul_k", "Rahul Kumar", "rahul.k@example.com", "DL-1420110012345", "+91-9876543210");
    TEST_ASSERT(cust.getUserId() == 1, "Customer ID is 1");
    TEST_ASSERT(cust.getUsername() == "rahul_k", "Username is rahul_k");
    TEST_ASSERT(cust.getFullName() == "Rahul Kumar", "Full name is Rahul Kumar");
    TEST_ASSERT(cust.getEmail() == "rahul.k@example.com", "Email is rahul.k@example.com");
    TEST_ASSERT(cust.getRole() == UserRole::Customer, "UserRole is Customer");
    TEST_ASSERT(cust.getDrivingLicense() == "DL-1420110012345", "License is DL-1420110012345");
    TEST_ASSERT(cust.getPhoneNumber() == "+91-9876543210", "Phone is +91-9876543210");
    TEST_ASSERT(cust.getRoleName() == "Customer", "Role name string is Customer");

    // Mutator
    cust.setPhoneNumber("+91-9999988888");
    TEST_ASSERT(cust.getPhoneNumber() == "+91-9999988888", "Phone number updated");

    cust.setEmail("rahul.new@company.in");
    TEST_ASSERT(cust.getEmail() == "rahul.new@company.in", "Email updated");
}

void testEmployeeConstructionAndGetters() {
    std::cout << "[RUNNING] testEmployeeConstructionAndGetters..." << std::endl;

    Employee emp(2, "priya_ops", "Priya Sharma", "priya.s@fleetflow.in", "EMP-204", "Fleet Operations");
    TEST_ASSERT(emp.getUserId() == 2, "Employee ID is 2");
    TEST_ASSERT(emp.getUsername() == "priya_ops", "Username is priya_ops");
    TEST_ASSERT(emp.getFullName() == "Priya Sharma", "Full name is Priya Sharma");
    TEST_ASSERT(emp.getEmail() == "priya.s@fleetflow.in", "Email is priya.s@fleetflow.in");
    TEST_ASSERT(emp.getRole() == UserRole::Employee, "UserRole is Employee");
    TEST_ASSERT(emp.getEmployeeCode() == "EMP-204", "Employee code is EMP-204");
    TEST_ASSERT(emp.getDepartment() == "Fleet Operations", "Department is Fleet Operations");
    TEST_ASSERT(emp.getRoleName() == "Employee", "Role name string is Employee");

    emp.setDepartment("Customer Desk");
    TEST_ASSERT(emp.getDepartment() == "Customer Desk", "Department updated to Customer Desk");
}

void testAdminConstructionAndGetters() {
    std::cout << "[RUNNING] testAdminConstructionAndGetters..." << std::endl;

    Admin admin(3, "aditya_adm", "Aditya Suryan", "admin@fleetflow.in", 3, "HQ - New Delhi");
    TEST_ASSERT(admin.getUserId() == 3, "Admin ID is 3");
    TEST_ASSERT(admin.getUsername() == "aditya_adm", "Username is aditya_adm");
    TEST_ASSERT(admin.getFullName() == "Aditya Suryan", "Full name is Aditya Suryan");
    TEST_ASSERT(admin.getRole() == UserRole::Admin, "UserRole is Admin");
    TEST_ASSERT(admin.getSecurityClearance() == 3, "Clearance level is 3");
    TEST_ASSERT(admin.getDesignatedOffice() == "HQ - New Delhi", "Office is HQ - New Delhi");
    TEST_ASSERT(admin.getRoleName() == "Admin", "Role name string is Admin");

    admin.setDesignatedOffice("Regional Hub - Bengaluru");
    TEST_ASSERT(admin.getDesignatedOffice() == "Regional Hub - Bengaluru", "Office updated to Bengaluru");
}

void testUserValidationRejections() {
    std::cout << "[RUNNING] testUserValidationRejections..." << std::endl;

    // 1. Invalid ID
    bool threw = false;
    try {
        Customer badId(0, "user", "Name", "u@mail.com", "DL-123", "98765");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "ID <= 0 throws ValidationException");

    // 2. Blank username
    threw = false;
    try {
        Customer blankUname(1, "   ", "Name", "u@mail.com", "DL-123", "98765");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank username throws ValidationException");

    // 3. Blank full name
    threw = false;
    try {
        Customer blankName(1, "user", " \t ", "u@mail.com", "DL-123", "98765");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank full name throws ValidationException");

    // 4. Invalid email formatting
    threw = false;
    try {
        Customer noAt(1, "user", "Name", "plainaddress", "DL-123", "98765");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Email without @ throws ValidationException");

    threw = false;
    try {
        Customer noDot(1, "user", "Name", "user@domain", "DL-123", "98765");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Email without domain dot throws ValidationException");

    threw = false;
    try {
        Customer atStart(1, "user", "Name", "@domain.com", "DL-123", "98765");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Email starting with @ throws ValidationException");

    // 5. Customer blank license and phone
    threw = false;
    try {
        Customer blankLic(1, "user", "Name", "u@mail.com", "", "98765");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank license throws ValidationException");

    threw = false;
    try {
        Customer blankPhone(1, "user", "Name", "u@mail.com", "DL-123", "  ");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank phone throws ValidationException");

    // 6. Employee blank code and dept
    threw = false;
    try {
        Employee blankEmpCode(2, "user", "Name", "u@mail.com", "  ", "Desk");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank employee code throws ValidationException");

    threw = false;
    try {
        Employee blankDept(2, "user", "Name", "u@mail.com", "EMP-1", "");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank department throws ValidationException");

    // 7. Admin clearance < 1 and blank office
    threw = false;
    try {
        Admin badClearance(3, "user", "Name", "u@mail.com", 0, "HQ");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Admin clearance < 1 throws ValidationException");

    threw = false;
    try {
        Admin blankOffice(3, "user", "Name", "u@mail.com", 1, "   ");
    } catch (const ValidationException&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Blank admin office throws ValidationException");
}

void testUserRuntimePolymorphism() {
    std::cout << "[RUNNING] testUserRuntimePolymorphism..." << std::endl;

    std::vector<std::unique_ptr<User>> users;
    users.push_back(std::make_unique<Customer>(101, "cust_arjun", "Arjun Reddy", "arjun@gmail.com", "DL-9988", "9988776655"));
    users.push_back(std::make_unique<Employee>(102, "emp_sneha", "Sneha Patel", "sneha@fleetflow.in", "EMP-007", "Desk Operations"));
    users.push_back(std::make_unique<Admin>(103, "adm_vikram", "Vikram Malhotra", "vikram@fleetflow.in", 2, "HQ - Mumbai"));

    TEST_ASSERT(users[0]->getRoleName() == "Customer", "Polymorphic role is Customer");
    TEST_ASSERT(users[1]->getRoleName() == "Employee", "Polymorphic role is Employee");
    TEST_ASSERT(users[2]->getRoleName() == "Admin", "Polymorphic role is Admin");

    // Stream profile rendering
    std::ostringstream oss;
    users[0]->displayProfile(oss);
    std::string custOut = oss.str();
    TEST_ASSERT(custOut.find("[USER PROFILE: CUSTOMER]") != std::string::npos, "Contains Customer header");
    TEST_ASSERT(custOut.find("Username        : cust_arjun") != std::string::npos, "Contains username");
    TEST_ASSERT(custOut.find("Driving License : DL-9988") != std::string::npos, "Contains driving license");

    std::ostringstream ossEmp;
    users[1]->displayProfile(ossEmp);
    std::string empOut = ossEmp.str();
    TEST_ASSERT(empOut.find("[USER PROFILE: EMPLOYEE]") != std::string::npos, "Contains Employee header");
    TEST_ASSERT(empOut.find("Employee Code   : EMP-007") != std::string::npos, "Contains employee code");

    std::ostringstream ossAdm;
    users[2]->displayProfile(ossAdm);
    std::string admOut = ossAdm.str();
    TEST_ASSERT(admOut.find("[USER PROFILE: ADMINISTRATOR]") != std::string::npos, "Contains Admin header");
    TEST_ASSERT(admOut.find("Clearance Level : Tier 2") != std::string::npos, "Contains clearance tier");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  FleetFlow Test Suite: User Hierarchy  " << std::endl;
    std::cout << "========================================" << std::endl;

    testCustomerConstructionAndGetters();
    testEmployeeConstructionAndGetters();
    testAdminConstructionAndGetters();
    testUserValidationRejections();
    testUserRuntimePolymorphism();

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed." << std::endl;
    std::cout << "========================================" << std::endl;

    return (g_testsFailed == 0) ? 0 : 1;
}

