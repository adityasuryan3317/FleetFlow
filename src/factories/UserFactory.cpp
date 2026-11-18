#include "fleetflow/factories/UserFactory.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/Employee.hpp"
#include "fleetflow/models/Admin.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>

namespace fleetflow::factories {

namespace {

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

void rejectWhitespace(const std::string& token, const std::string& fieldName) {
    for (char c : token) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            throw core::StorageException(
                fieldName + " contains whitespace: '" + token + "'");
        }
    }
}

int parseInt(const std::string& token, const std::string& fieldName) {
    rejectWhitespace(token, fieldName);
    if (token.empty()) {
        throw core::StorageException(fieldName + " is empty.");
    }
    try {
        std::size_t pos = 0;
        long long val = std::stoll(token, &pos);
        if (pos != token.size()) {
            throw core::StorageException(
                fieldName + " contains trailing characters: '" + token + "'");
        }
        if (val < std::numeric_limits<int>::min() || val > std::numeric_limits<int>::max()) {
            throw core::StorageException(
                fieldName + " out of int range: '" + token + "'");
        }
        return static_cast<int>(val);
    } catch (const core::StorageException&) {
        throw;
    } catch (const std::exception& e) {
        throw core::StorageException(
            "Failed to parse " + fieldName + " '" + token + "': " + e.what());
    }
}

void validateTokenCount(const std::vector<std::string>& tokens,
                        std::size_t expected, const std::string& role) {
    if (tokens.size() != expected) {
        throw core::StorageException(
            role + " requires exactly " + std::to_string(expected) +
            " tokens, but received " + std::to_string(tokens.size()) + ".");
    }
}

// ============================================================================
// Concrete User Creators
// ============================================================================

std::unique_ptr<models::User> createCustomer(const std::vector<std::string>& tokens) {
    validateTokenCount(tokens, 6, "Customer");
    try {
        int id = parseInt(tokens[0], "id");
        return std::make_unique<models::Customer>(
            id, tokens[1], tokens[2], tokens[3], tokens[4], tokens[5]);
    } catch (const core::StorageException&) {
        throw;
    } catch (const core::FleetFlowException& e) {
        throw core::StorageException(
            "Failed to construct Customer: " + std::string(e.what()));
    }
}

std::unique_ptr<models::User> createEmployee(const std::vector<std::string>& tokens) {
    validateTokenCount(tokens, 6, "Employee");
    try {
        int id = parseInt(tokens[0], "id");
        return std::make_unique<models::Employee>(
            id, tokens[1], tokens[2], tokens[3], tokens[4], tokens[5]);
    } catch (const core::StorageException&) {
        throw;
    } catch (const core::FleetFlowException& e) {
        throw core::StorageException(
            "Failed to construct Employee: " + std::string(e.what()));
    }
}

std::unique_ptr<models::User> createAdmin(const std::vector<std::string>& tokens) {
    validateTokenCount(tokens, 6, "Admin");
    try {
        int id = parseInt(tokens[0], "id");
        int clearance = parseInt(tokens[4], "securityClearance");
        return std::make_unique<models::Admin>(
            id, tokens[1], tokens[2], tokens[3], clearance, tokens[5]);
    } catch (const core::StorageException&) {
        throw;
    } catch (const core::FleetFlowException& e) {
        throw core::StorageException(
            "Failed to construct Admin: " + std::string(e.what()));
    }
}

} // anonymous namespace

std::unique_ptr<models::User> UserFactory::createFromTokens(
    const std::string& role,
    const std::vector<std::string>& tokens) {

    std::string lower = toLower(role);
    if (lower == "customer") return createCustomer(tokens);
    if (lower == "employee") return createEmployee(tokens);
    if (lower == "admin")    return createAdmin(tokens);

    throw core::StorageException("Unknown user role: '" + role + "'");
}

} // namespace fleetflow::factories

