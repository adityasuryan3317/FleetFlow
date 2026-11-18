#pragma once

#include "fleetflow/models/User.hpp"
#include <memory>
#include <string>
#include <vector>

namespace fleetflow::factories {

/**
 * @brief Factory for constructing polymorphic User objects from CSV token vectors.
 *
 * Token Contract (role discriminator supplied separately):
 * - Customer tokens [6]: id, username, fullName, email, drivingLicense, phoneNumber
 * - Employee tokens [6]: id, username, fullName, email, employeeCode, department
 * - Admin tokens [6]:    id, username, fullName, email, securityClearance, designatedOffice
 *
 * @note Role matching is case-insensitive ("CUSTOMER", "customer", "Customer" all match).
 * @note All numeric tokens must not contain any whitespace characters.
 * @note Domain-level validation errors from model constructors are translated to StorageException.
 */
class UserFactory {
public:
    /**
     * @brief Creates a User from a role discriminator and token vector.
     * @param role User role string ("CUSTOMER", "EMPLOYEE", or "ADMIN"), case-insensitive.
     * @param tokens CSV field tokens (excluding the role discriminator).
     * @return std::unique_ptr<models::User> owning the constructed user.
     * @throws core::StorageException on unknown role, wrong token count, invalid numeric format,
     *         or domain validation failure.
     */
    static std::unique_ptr<models::User> createFromTokens(
        const std::string& role,
        const std::vector<std::string>& tokens);
};

} // namespace fleetflow::factories

