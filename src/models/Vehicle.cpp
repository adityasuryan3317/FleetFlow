#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include <iostream>

namespace fleetflow::models {

namespace {

bool isBlank(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

} // anonymous namespace

Vehicle::Vehicle(int id,
                 std::string brandName,
                 std::string modelName,
                 std::string plate,
                 double dailyRate,
                 core::VehicleStatus initialStatus)
    : status(initialStatus) {

    if (id <= 0) {
        throw core::ValidationException("Vehicle ID must be a positive integer, received: " + std::to_string(id));
    }
    if (isBlank(brandName)) {
        throw core::ValidationException("Vehicle brand cannot be empty or whitespace.");
    }
    if (isBlank(modelName)) {
        throw core::ValidationException("Vehicle model cannot be empty or whitespace.");
    }
    if (isBlank(plate)) {
        throw core::ValidationException("Vehicle license plate cannot be empty or whitespace.");
    }
    if (dailyRate <= 0.0) {
        throw core::ValidationException("Base daily rate must be positive, received: " + std::to_string(dailyRate));
    }

    vehicleId = id;
    brand = std::move(brandName);
    model = std::move(modelName);
    licensePlate = std::move(plate);
    baseDailyRate = dailyRate;
}

void Vehicle::setBaseDailyRate(double newRate) {
    if (newRate <= 0.0) {
        throw core::ValidationException("Base daily rate must be positive, received: " + std::to_string(newRate));
    }
    baseDailyRate = newRate;
}

void Vehicle::displayDetails() const {
    displayDetails(std::cout);
}

} // namespace fleetflow::models

