#include "fleetflow/models/Employee.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include <ostream>

namespace fleetflow::models {

namespace {

bool isBlank(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

} // anonymous namespace

Employee::Employee(int id,
                   std::string uname,
                   std::string name,
                   std::string mail,
                   std::string empCode,
                   std::string dept)
    : User(id, std::move(uname), std::move(name), std::move(mail), core::UserRole::Employee) {

    if (isBlank(empCode)) {
        throw core::ValidationException("Employee code cannot be empty or whitespace.");
    }
    if (isBlank(dept)) {
        throw core::ValidationException("Employee department cannot be empty or whitespace.");
    }

    employeeCode = std::move(empCode);
    department = std::move(dept);
}

void Employee::setDepartment(const std::string& newDept) {
    if (isBlank(newDept)) {
        throw core::ValidationException("Employee department cannot be empty or whitespace.");
    }
    department = newDept;
}

std::string Employee::getRoleName() const {
    return "Employee";
}

void Employee::displayProfile(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [USER PROFILE: EMPLOYEE]\n"
       << " User ID         : " << getUserId() << "\n"
       << " Username        : " << getUsername() << "\n"
       << " Full Name       : " << getFullName() << "\n"
       << " Email           : " << getEmail() << "\n"
       << " Employee Code   : " << employeeCode << "\n"
       << " Department      : " << department << "\n"
       << " System Role     : " << core::toString(getRole()) << "\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

