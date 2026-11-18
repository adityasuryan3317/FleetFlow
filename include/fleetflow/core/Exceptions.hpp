#pragma once

#include <stdexcept>
#include <string>

namespace fleetflow::core {

/**
 * @brief Base exception class for all domain errors within FleetFlow.
 */
class FleetFlowException : public std::runtime_error {
public:
    explicit FleetFlowException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Thrown when input validation fails for domain entity attributes.
 */
class ValidationException : public FleetFlowException {
public:
    explicit ValidationException(const std::string& message)
        : FleetFlowException("Validation Error: " + message) {}
};

/**
 * @brief Thrown when a Date fails calendar validity checks.
 */
class InvalidDateException : public FleetFlowException {
public:
    explicit InvalidDateException(const std::string& message)
        : FleetFlowException("Invalid Date: " + message) {}
};

/**
 * @brief Thrown when a DateRange is logically malformed (e.g. endDate < startDate).
 */
class InvalidDateRangeException : public FleetFlowException {
public:
    explicit InvalidDateRangeException(const std::string& message)
        : FleetFlowException("Invalid Date Range: " + message) {}
};

/**
 * @brief Thrown when attempting to book a vehicle that is not available for requested dates.
 */
class VehicleUnavailableException : public FleetFlowException {
public:
    explicit VehicleUnavailableException(const std::string& message)
        : FleetFlowException("Vehicle Unavailable: " + message) {}
};

/**
 * @brief Thrown when a requested entity cannot be found by its identifier.
 */
class EntityNotFoundException : public FleetFlowException {
public:
    explicit EntityNotFoundException(const std::string& message)
        : FleetFlowException("Entity Not Found: " + message) {}
};

/**
 * @brief Thrown when attempting to insert an entity with an ID or key that already exists.
 */
class DuplicateEntityException : public FleetFlowException {
public:
    explicit DuplicateEntityException(const std::string& message)
        : FleetFlowException("Duplicate Entity: " + message) {}
};

/**
 * @brief Thrown when a user attempts an action unauthorized for their UserRole.
 */
class UnauthorizedAccessException : public FleetFlowException {
public:
    explicit UnauthorizedAccessException(const std::string& message)
        : FleetFlowException("Unauthorized Access: " + message) {}
};

/**
 * @brief Thrown during file persistence failures or corrupted records.
 */
class StorageException : public FleetFlowException {
public:
    explicit StorageException(const std::string& message)
        : FleetFlowException("Storage Error: " + message) {}
};

/**
 * @brief Thrown when an illegal state transition or domain operation is attempted.
 */
class InvalidOperationException : public FleetFlowException {
public:
    explicit InvalidOperationException(const std::string& message)
        : FleetFlowException("Invalid Operation: " + message) {}
};

} // namespace fleetflow::core

