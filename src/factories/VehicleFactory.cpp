#include "fleetflow/factories/VehicleFactory.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Bike.hpp"
#include "fleetflow/models/Truck.hpp"
#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
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

double parseDouble(const std::string& token, const std::string& fieldName) {
    rejectWhitespace(token, fieldName);
    if (token.empty()) {
        throw core::StorageException(fieldName + " is empty.");
    }
    std::string lower = token;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (lower.find("nan") != std::string::npos || lower.find("inf") != std::string::npos) {
        throw core::StorageException(
            fieldName + " must be finite (cannot be NaN or Infinity): '" + token + "'");
    }

    std::istringstream iss(token);
    iss.imbue(std::locale::classic());
    double val = 0.0;
    iss >> val;
    if (iss.fail() || !iss.eof()) {
        throw core::StorageException(
            fieldName + " contains invalid characters: '" + token + "'");
    }
    if (!std::isfinite(val)) {
        throw core::StorageException(
            fieldName + " must be finite (cannot be NaN or Infinity): '" + token + "'");
    }
    return val;
}

bool parseBool(const std::string& token, const std::string& fieldName) {
    std::string lower = toLower(token);
    if (lower == "true" || lower == "1" || lower == "yes") return true;
    if (lower == "false" || lower == "0" || lower == "no") return false;
    throw core::StorageException(
        fieldName + " is not a valid boolean: '" + token + "'");
}

/// @brief Case-insensitive VehicleStatus parsing, delegating to core::vehicleStatusFromString.
core::VehicleStatus parseVehicleStatus(const std::string& token) {
    std::string lower = toLower(token);

    // Map lowercase to canonical form for the existing converter
    std::string canonical;
    if (lower == "available")        canonical = "Available";
    else if (lower == "undermaintenance") canonical = "UnderMaintenance";
    else if (lower == "retired")     canonical = "Retired";
    else {
        throw core::StorageException("Unknown vehicle status: '" + token + "'");
    }

    return core::vehicleStatusFromString(canonical);
}

void validateTokenCount(const std::vector<std::string>& tokens,
                        std::size_t expected, const std::string& type) {
    if (tokens.size() != expected) {
        throw core::StorageException(
            type + " requires exactly " + std::to_string(expected) +
            " tokens, but received " + std::to_string(tokens.size()) + ".");
    }
}

// ============================================================================
// Concrete Vehicle Creators
// ============================================================================

std::unique_ptr<models::Vehicle> createCar(const std::vector<std::string>& tokens) {
    validateTokenCount(tokens, 9, "Car");
    try {
        int id             = parseInt(tokens[0], "id");
        double rate        = parseDouble(tokens[4], "baseDailyRate");
        auto status        = parseVehicleStatus(tokens[5]);
        int seats          = parseInt(tokens[6], "seatingCapacity");
        bool automatic     = parseBool(tokens[8], "isAutomatic");

        return std::make_unique<models::Car>(
            id, tokens[1], tokens[2], tokens[3], rate,
            seats, tokens[7], automatic, status);
    } catch (const core::StorageException&) {
        throw;
    } catch (const core::FleetFlowException& e) {
        throw core::StorageException(
            "Failed to construct Car: " + std::string(e.what()));
    }
}

std::unique_ptr<models::Vehicle> createBike(const std::vector<std::string>& tokens) {
    validateTokenCount(tokens, 8, "Bike");
    try {
        int id          = parseInt(tokens[0], "id");
        double rate     = parseDouble(tokens[4], "baseDailyRate");
        auto status     = parseVehicleStatus(tokens[5]);
        int cc          = parseInt(tokens[6], "engineCC");
        bool geared     = parseBool(tokens[7], "hasGear");

        return std::make_unique<models::Bike>(
            id, tokens[1], tokens[2], tokens[3], rate,
            cc, geared, status);
    } catch (const core::StorageException&) {
        throw;
    } catch (const core::FleetFlowException& e) {
        throw core::StorageException(
            "Failed to construct Bike: " + std::string(e.what()));
    }
}

std::unique_ptr<models::Vehicle> createTruck(const std::vector<std::string>& tokens) {
    validateTokenCount(tokens, 8, "Truck");
    try {
        int id             = parseInt(tokens[0], "id");
        double rate        = parseDouble(tokens[4], "baseDailyRate");
        auto status        = parseVehicleStatus(tokens[5]);
        double capacity    = parseDouble(tokens[6], "cargoCapacityTons");
        int axles          = parseInt(tokens[7], "numberOfAxles");

        return std::make_unique<models::Truck>(
            id, tokens[1], tokens[2], tokens[3], rate,
            capacity, axles, status);
    } catch (const core::StorageException&) {
        throw;
    } catch (const core::FleetFlowException& e) {
        throw core::StorageException(
            "Failed to construct Truck: " + std::string(e.what()));
    }
}

} // anonymous namespace

std::unique_ptr<models::Vehicle> VehicleFactory::createFromTokens(
    const std::string& type,
    const std::vector<std::string>& tokens) {

    std::string lower = toLower(type);
    if (lower == "car")   return createCar(tokens);
    if (lower == "bike")  return createBike(tokens);
    if (lower == "truck") return createTruck(tokens);

    throw core::StorageException("Unknown vehicle type: '" + type + "'");
}

} // namespace fleetflow::factories

