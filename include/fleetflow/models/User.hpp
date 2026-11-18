#pragma once

#include "fleetflow/core/CommonTypes.hpp"
#include <string>
#include <iosfwd>

namespace fleetflow::models {

/**
 * @brief Abstract base class representing a registered system user.
 * Encapsulates identity and credential metadata for Role-Based Access Control (RBAC).
 */
class User {
private:
    int userId{0};
    std::string username;
    std::string fullName;
    std::string email;
    core::UserRole role{core::UserRole::Customer};

public:
    User(int id,
         std::string uname,
         std::string name,
         std::string mail,
         core::UserRole userRole);

    virtual ~User() = default;

    // Accessors
    int getUserId() const noexcept { return userId; }
    const std::string& getUsername() const noexcept { return username; }
    const std::string& getFullName() const noexcept { return fullName; }
    const std::string& getEmail() const noexcept { return email; }
    core::UserRole getRole() const noexcept { return role; }

    // Mutators
    void setEmail(const std::string& newEmail);
    void setFullName(const std::string& newName);

    // Pure Virtual Interfaces
    virtual std::string getRoleName() const = 0;
    virtual void displayProfile(std::ostream& os) const = 0;

    // Helper for basic application-level email validation
    static bool isValidEmail(const std::string& mail);
};

} // namespace fleetflow::models

