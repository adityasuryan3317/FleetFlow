#pragma once

#include "fleetflow/models/Vehicle.hpp"

namespace fleetflow::models {

namespace pricing {
    /// Engine displacement threshold for premium high-performance motorcycles (cc)
    constexpr int BIKE_PREMIUM_CC_THRESHOLD = 350;
    /// Daily surcharge for premium high-displacement motorcycles
    constexpr double BIKE_PREMIUM_ENGINE_SURCHARGE = 15.0;
    /// Daily discount for gearless commuter scooters
    constexpr double BIKE_SCOOTER_DISCOUNT = 5.0;
    /// Absolute floor rate for any two-wheeler
    constexpr double BIKE_MINIMUM_DAILY_RATE = 5.0;
} // namespace pricing

/**
 * @brief Concrete derived class representing motorcycles, cruisers, and gearless scooters.
 */
class Bike : public Vehicle {
private:
    int engineCC{150};
    bool hasGear{true};

public:
    Bike(int id,
         std::string brandName,
         std::string modelName,
         std::string plate,
         double dailyRate,
         int cc,
         bool geared,
         core::VehicleStatus initialStatus = core::VehicleStatus::Available);

    // Accessors
    int getEngineCC() const noexcept { return engineCC; }
    bool getHasGear() const noexcept { return hasGear; }

    // Polymorphic overrides
    double calculateDailyRate() const override;
    std::string getVehicleType() const override;
    void displayDetails(std::ostream& os) const override;
};

} // namespace fleetflow::models
