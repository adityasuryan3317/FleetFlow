#pragma once

#include "fleetflow/core/CommonTypes.hpp"
#include <string>
#include <iosfwd>

namespace fleetflow::models {

/**
 * @brief Abstract base class representing a general fleet vehicle.
 * Encapsulates core vehicle properties and establishes pure virtual interfaces
 * for polymorphic pricing, typing, and formatted inspection.
 */
class Vehicle {
private:
    int vehicleId{0};
    std::string brand;
    std::string model;
    std::string licensePlate;
    double baseDailyRate{0.0};
    core::VehicleStatus status{core::VehicleStatus::Available};

public:
    Vehicle(int id,
            std::string brandName,
            std::string modelName,
            std::string plate,
            double dailyRate,
            core::VehicleStatus initialStatus = core::VehicleStatus::Available);

    virtual ~Vehicle() = default;

    // Accessors
    int getVehicleId() const noexcept { return vehicleId; }
    const std::string& getBrand() const noexcept { return brand; }
    const std::string& getModel() const noexcept { return model; }
    const std::string& getLicensePlate() const noexcept { return licensePlate; }
    double getBaseDailyRate() const noexcept { return baseDailyRate; }
    core::VehicleStatus getStatus() const noexcept { return status; }

    // Mutators
    void setStatus(core::VehicleStatus newStatus) noexcept { status = newStatus; }
    void setBaseDailyRate(double newRate);

    // Pure Virtual Interfaces
    virtual double calculateDailyRate() const = 0;
    virtual std::string getVehicleType() const = 0;
    virtual void displayDetails(std::ostream& os) const = 0;

    // Convenience overload for default standard output
    void displayDetails() const;
};

} // namespace fleetflow::models

