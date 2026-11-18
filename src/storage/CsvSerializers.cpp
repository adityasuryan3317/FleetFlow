#include "fleetflow/storage/CsvSerializers.hpp"
#include "fleetflow/factories/VehicleFactory.hpp"
#include "fleetflow/factories/UserFactory.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Bike.hpp"
#include "fleetflow/models/Truck.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/Employee.hpp"
#include "fleetflow/models/Admin.hpp"
#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/DateRange.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace fleetflow::storage {

namespace {

// ============================================================================
// Formatting & Strict Parsing Helpers
// ============================================================================

std::string toUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::toupper(c); });
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

int parseIntStrict(const std::string& token, const std::string& fieldName) {
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
                fieldName + " out of integer range: '" + token + "'");
        }
        return static_cast<int>(val);
    } catch (const core::StorageException&) {
        throw;
    } catch (const std::exception& e) {
        throw core::StorageException(
            "Failed to parse " + fieldName + " '" + token + "': " + e.what());
    }
}

double parseDoubleStrict(const std::string& token, const std::string& fieldName) {
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
            fieldName + " must be a finite number (rejecting NaN/Infinity): '" + token + "'");
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
            fieldName + " must be a finite number (rejecting NaN/Infinity): '" + token + "'");
    }
    return val;
}

std::string formatDouble(double val) {
    if (!std::isfinite(val)) {
        throw core::StorageException("Cannot serialize non-finite double value (NaN or Infinity).");
    }
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::defaultfloat << std::setprecision(std::numeric_limits<double>::max_digits10) << val;
    return oss.str();
}

} // anonymous namespace

// ============================================================================
// COMPONENT 1: VEHICLE SERIALIZER
// Schema: id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3 (10)
// ============================================================================

std::vector<std::string> VehicleSerializer::serialize(const models::Vehicle& vehicle) {
    if (const auto* car = dynamic_cast<const models::Car*>(&vehicle)) {
        return {
            std::to_string(car->getVehicleId()),
            "CAR",
            car->getBrand(),
            car->getModel(),
            car->getLicensePlate(),
            formatDouble(car->getBaseDailyRate()),
            core::toString(car->getStatus()),
            std::to_string(car->getSeatingCapacity()),
            car->getFuelType(),
            car->getIsAutomatic() ? "true" : "false"
        };
    } else if (const auto* bike = dynamic_cast<const models::Bike*>(&vehicle)) {
        return {
            std::to_string(bike->getVehicleId()),
            "BIKE",
            bike->getBrand(),
            bike->getModel(),
            bike->getLicensePlate(),
            formatDouble(bike->getBaseDailyRate()),
            core::toString(bike->getStatus()),
            std::to_string(bike->getEngineCC()),
            bike->getHasGear() ? "true" : "false",
            ""
        };
    } else if (const auto* truck = dynamic_cast<const models::Truck*>(&vehicle)) {
        return {
            std::to_string(truck->getVehicleId()),
            "TRUCK",
            truck->getBrand(),
            truck->getModel(),
            truck->getLicensePlate(),
            formatDouble(truck->getBaseDailyRate()),
            core::toString(truck->getStatus()),
            formatDouble(truck->getCargoCapacityTons()),
            std::to_string(truck->getNumberOfAxles()),
            ""
        };
    }
    throw core::StorageException("Unknown vehicle dynamic type in VehicleSerializer::serialize");
}

std::unique_ptr<models::Vehicle> VehicleSerializer::deserialize(const std::vector<std::string>& fields) {
    if (fields.size() != 10) {
        throw core::StorageException(
            "Vehicle CSV row requires exactly 10 fields, received " + std::to_string(fields.size()) + ".");
    }

    const std::string& type = fields[1];
    std::string upperType = toUpper(type);

    std::vector<std::string> tokens;
    if (upperType == "CAR") {
        // Car tokens [9]: id, brand, model, licensePlate, baseDailyRate, status, seatingCapacity, fuelType, isAutomatic
        tokens = {fields[0], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], fields[8], fields[9]};
    } else if (upperType == "BIKE") {
        // Bike tokens [8]: id, brand, model, licensePlate, baseDailyRate, status, engineCC, hasGear
        tokens = {fields[0], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], fields[8]};
    } else if (upperType == "TRUCK") {
        // Truck tokens [8]: id, brand, model, licensePlate, baseDailyRate, status, cargoCapacityTons, numberOfAxles
        tokens = {fields[0], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], fields[8]};
    } else {
        throw core::StorageException("Unknown vehicle type in VehicleSerializer: '" + type + "'");
    }

    return factories::VehicleFactory::createFromTokens(type, tokens);
}

// ============================================================================
// COMPONENT 2: USER SERIALIZER
// Schema: id,role,username,fullName,email,extra1,extra2 (7)
// ============================================================================

std::vector<std::string> UserSerializer::serialize(const models::User& user) {
    if (const auto* cust = dynamic_cast<const models::Customer*>(&user)) {
        return {
            std::to_string(cust->getUserId()),
            "CUSTOMER",
            cust->getUsername(),
            cust->getFullName(),
            cust->getEmail(),
            cust->getDrivingLicense(),
            cust->getPhoneNumber()
        };
    } else if (const auto* emp = dynamic_cast<const models::Employee*>(&user)) {
        return {
            std::to_string(emp->getUserId()),
            "EMPLOYEE",
            emp->getUsername(),
            emp->getFullName(),
            emp->getEmail(),
            emp->getEmployeeCode(),
            emp->getDepartment()
        };
    } else if (const auto* adm = dynamic_cast<const models::Admin*>(&user)) {
        return {
            std::to_string(adm->getUserId()),
            "ADMIN",
            adm->getUsername(),
            adm->getFullName(),
            adm->getEmail(),
            std::to_string(adm->getSecurityClearance()),
            adm->getDesignatedOffice()
        };
    }
    throw core::StorageException("Unknown user dynamic type in UserSerializer::serialize");
}

std::unique_ptr<models::User> UserSerializer::deserialize(const std::vector<std::string>& fields) {
    if (fields.size() != 7) {
        throw core::StorageException(
            "User CSV row requires exactly 7 fields, received " + std::to_string(fields.size()) + ".");
    }

    const std::string& role = fields[1];
    // User tokens [6]: id, username, fullName, email, extra1, extra2
    std::vector<std::string> tokens = {
        fields[0], fields[2], fields[3], fields[4], fields[5], fields[6]
    };

    return factories::UserFactory::createFromTokens(role, tokens);
}

// ============================================================================
// COMPONENT 3: RENTAL SERIALIZER
// Schema: rentalId,customerId,vehicleId,startDate,scheduledEndDate,actualReturnDate,estimatedCost,finalCost,status,actualStartDate (10)
// ============================================================================

std::vector<std::string> RentalSerializer::serialize(const models::RentalRecord& record) {
    return {
        std::to_string(record.getRentalId()),
        std::to_string(record.getCustomerId()),
        std::to_string(record.getVehicleId()),
        record.getScheduledRange().getStartDate().toString(),
        record.getScheduledRange().getEndDate().toString(),
        record.getActualReturnDate().has_value() ? record.getActualReturnDate()->toString() : "",
        formatDouble(record.getEstimatedCost()),
        record.getFinalCost().has_value() ? formatDouble(*record.getFinalCost()) : "",
        core::toString(record.getStatus()),
        record.getActualStartDate().has_value() ? record.getActualStartDate()->toString() : ""
    };
}

models::RentalRecord RentalSerializer::deserialize(const std::vector<std::string>& fields) {
    if (fields.size() != 9 && fields.size() != 10) {
        throw core::StorageException(
            "RentalRecord CSV row requires 9 or 10 fields, received " + std::to_string(fields.size()) + ".");
    }

    int rId = parseIntStrict(fields[0], "rentalId");
    int cId = parseIntStrict(fields[1], "customerId");
    int vId = parseIntStrict(fields[2], "vehicleId");

    core::Date start;
    try {
        start = core::Date::fromString(fields[3]);
    } catch (const std::exception& e) {
        throw core::StorageException("Invalid startDate in RentalRecord: " + std::string(e.what()));
    }

    core::Date end;
    try {
        end = core::Date::fromString(fields[4]);
    } catch (const std::exception& e) {
        throw core::StorageException("Invalid scheduledEndDate in RentalRecord: " + std::string(e.what()));
    }

    core::DateRange range(start, end);

    std::optional<core::Date> actualReturn;
    if (!fields[5].empty()) {
        try {
            actualReturn = core::Date::fromString(fields[5]);
        } catch (const std::exception& e) {
            throw core::StorageException("Invalid actualReturnDate in RentalRecord: " + std::string(e.what()));
        }
    }

    double estCost = parseDoubleStrict(fields[6], "estimatedCost");

    std::optional<double> finCost;
    if (!fields[7].empty()) {
        finCost = parseDoubleStrict(fields[7], "finalCost");
    }

    core::RentalStatus stat;
    try {
        stat = core::rentalStatusFromString(fields[8]);
    } catch (const std::exception& e) {
        throw core::StorageException("Invalid rental status in RentalRecord: " + std::string(e.what()));
    }

    std::optional<core::Date> actualStart;
    if (fields.size() == 10 && !fields[9].empty()) {
        try {
            actualStart = core::Date::fromString(fields[9]);
        } catch (const std::exception& e) {
            throw core::StorageException("Invalid actualStartDate in RentalRecord: " + std::string(e.what()));
        }
    }

    return models::RentalRecord::restoreFromStorage(rId, cId, vId, range, actualStart, actualReturn, estCost, finCost, stat);
}

// ============================================================================
// COMPONENT 4: PAYMENT SERIALIZER
// Schema: paymentId,rentalId,customerId,amount,date,type,method,description (8)
// ============================================================================

std::vector<std::string> PaymentSerializer::serialize(const models::PaymentRecord& record) {
    return {
        std::to_string(record.getPaymentId()),
        std::to_string(record.getRentalId()),
        std::to_string(record.getCustomerId()),
        formatDouble(record.getAmount()),
        record.getTransactionDate().toString(),
        core::toString(record.getPaymentType()),
        core::toString(record.getPaymentMethod()),
        record.getDescription()
    };
}

models::PaymentRecord PaymentSerializer::deserialize(const std::vector<std::string>& fields) {
    if (fields.size() != 8) {
        throw core::StorageException(
            "PaymentRecord CSV row requires exactly 8 fields, received " + std::to_string(fields.size()) + ".");
    }

    int pId = parseIntStrict(fields[0], "paymentId");
    int rId = parseIntStrict(fields[1], "rentalId");
    int cId = parseIntStrict(fields[2], "customerId");
    double amt = parseDoubleStrict(fields[3], "amount");

    core::Date transDate;
    try {
        transDate = core::Date::fromString(fields[4]);
    } catch (const std::exception& e) {
        throw core::StorageException("Invalid transactionDate in PaymentRecord: " + std::string(e.what()));
    }

    core::PaymentType pType;
    try {
        pType = core::paymentTypeFromString(fields[5]);
    } catch (const std::exception& e) {
        throw core::StorageException("Invalid paymentType in PaymentRecord: " + std::string(e.what()));
    }

    core::PaymentMethod pMethod;
    try {
        pMethod = core::paymentMethodFromString(fields[6]);
    } catch (const std::exception& e) {
        throw core::StorageException("Invalid paymentMethod in PaymentRecord: " + std::string(e.what()));
    }

    try {
        return models::PaymentRecord(pId, rId, cId, amt, transDate, pType, pMethod, fields[7]);
    } catch (const core::FleetFlowException& e) {
        throw core::StorageException("Failed to construct PaymentRecord: " + std::string(e.what()));
    }
}

// ============================================================================
// COMPONENT 5: MAINTENANCE SERIALIZER
// Schema: maintenanceId,vehicleId,problemDescription,startDate,completionDate,cost,status (7)
// ============================================================================

std::vector<std::string> MaintenanceSerializer::serialize(const models::MaintenanceRecord& record) {
    return {
        std::to_string(record.getMaintenanceId()),
        std::to_string(record.getVehicleId()),
        record.getProblemDescription(),
        record.getStartDate().toString(),
        record.getCompletionDate().has_value() ? record.getCompletionDate()->toString() : "",
        formatDouble(record.getCost()),
        core::toString(record.getStatus())
    };
}

models::MaintenanceRecord MaintenanceSerializer::deserialize(const std::vector<std::string>& fields) {
    if (fields.size() != 7) {
        throw core::StorageException(
            "MaintenanceRecord CSV row requires exactly 7 fields, received " + std::to_string(fields.size()) + ".");
    }

    int mId = parseIntStrict(fields[0], "maintenanceId");
    int vId = parseIntStrict(fields[1], "vehicleId");
    std::string desc = fields[2];

    core::Date start;
    try {
        start = core::Date::fromString(fields[3]);
    } catch (const std::exception& e) {
        throw core::StorageException("Invalid startDate in MaintenanceRecord: " + std::string(e.what()));
    }

    std::optional<core::Date> completion;
    if (!fields[4].empty()) {
        try {
            completion = core::Date::fromString(fields[4]);
        } catch (const std::exception& e) {
            throw core::StorageException("Invalid completionDate in MaintenanceRecord: " + std::string(e.what()));
        }
    }

    double cost = parseDoubleStrict(fields[5], "cost");

    core::MaintenanceStatus stat;
    try {
        stat = core::maintenanceStatusFromString(fields[6]);
    } catch (const std::exception& e) {
        throw core::StorageException("Invalid maintenance status in MaintenanceRecord: " + std::string(e.what()));
    }

    return models::MaintenanceRecord::restoreFromStorage(mId, vId, std::move(desc), start, completion, cost, stat);
}

} // namespace fleetflow::storage

