#pragma once

#include "fleetflow/models/User.hpp"

namespace fleetflow::models {

/**
 * @brief Concrete derived class representing an executive system administrator.
 */
class Admin : public User {
private:
    int securityClearance{1};
    std::string designatedOffice;

public:
    Admin(int id,
          std::string uname,
          std::string name,
          std::string mail,
          int clearanceLevel,
          std::string office = "Headquarters");

    // Accessors
    int getSecurityClearance() const noexcept { return securityClearance; }
    const std::string& getDesignatedOffice() const noexcept { return designatedOffice; }

    // Mutator
    void setDesignatedOffice(const std::string& newOffice);

    // Polymorphic Overrides
    std::string getRoleName() const override;
    void displayProfile(std::ostream& os) const override;
};

} // namespace fleetflow::models

