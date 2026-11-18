#pragma once

#include "fleetflow/models/User.hpp"

namespace fleetflow::models {

/**
 * @brief Concrete derived class representing a registered rental customer.
 */
class Customer : public User {
private:
    std::string drivingLicense;
    std::string phoneNumber;

public:
    Customer(int id,
             std::string uname,
             std::string name,
             std::string mail,
             std::string license,
             std::string phone);

    // Accessors
    const std::string& getDrivingLicense() const noexcept { return drivingLicense; }
    const std::string& getPhoneNumber() const noexcept { return phoneNumber; }

    // Mutator
    void setPhoneNumber(const std::string& newPhone);

    // Polymorphic Overrides
    std::string getRoleName() const override;
    void displayProfile(std::ostream& os) const override;
};

} // namespace fleetflow::models

