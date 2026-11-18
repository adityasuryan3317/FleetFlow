#pragma once

#include "fleetflow/models/Vehicle.hpp"
#include <memory>
#include <string>
#include <vector>

namespace fleetflow::factories {

/**
 * @brief Factory for constructing polymorphic Vehicle objects from CSV token vectors.
 *
 * Token Contract (type discriminator supplied separately):
 * - Car tokens [9]:   id, brand, model, licensePlate, baseDailyRate, status, seatingCapacity, fuelType, isAutomatic
 * - Bike tokens [8]:  id, brand, model, licensePlate, baseDailyRate, status, engineCC, hasGear
 * - Truck tokens [8]: id, brand, model, licensePlate, baseDailyRate, status, cargoCapacityTons, numberOfAxles
 *
 * @note Type matching is case-insensitive ("CAR", "car", "Car" all match).
 * @note All numeric tokens must not contain any whitespace characters.
 * @note Domain-level validation errors from model constructors are translated to StorageException.
 */
class VehicleFactory {
public:
    /**
     * @brief Creates a Vehicle from a type discriminator and token vector.
     * @param type Vehicle type string ("CAR", "BIKE", or "TRUCK"), case-insensitive.
     * @param tokens CSV field tokens (excluding the type discriminator).
     * @return std::unique_ptr<models::Vehicle> owning the constructed vehicle.
     * @throws core::StorageException on unknown type, wrong token count, invalid numeric format,
     *         or domain validation failure.
     */
    static std::unique_ptr<models::Vehicle> createFromTokens(
        const std::string& type,
        const std::vector<std::string>& tokens);
};

} // namespace fleetflow::factories

