#pragma once

#include "fleetflow/models/User.hpp"

namespace fleetflow::models {

/**
 * @brief Concrete derived class representing an employee / staff member.
 */
class Employee : public User {
private:
    std::string employeeCode;
    std::string department;

public:
    Employee(int id,
             std::string uname,
             std::string name,
             std::string mail,
             std::string empCode,
             std::string dept);

    // Accessors
    const std::string& getEmployeeCode() const noexcept { return employeeCode; }
    const std::string& getDepartment() const noexcept { return department; }

    // Mutator
    void setDepartment(const std::string& newDept);

    // Polymorphic Overrides
    std::string getRoleName() const override;
    void displayProfile(std::ostream& os) const override;
};

} // namespace fleetflow::models

