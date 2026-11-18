#pragma once

#include "fleetflow/models/Vehicle.hpp"

namespace fleetflow::models {

namespace pricing {
    /// Daily surcharge for automatic transmission convenience
    constexpr double CAR_AUTOMATIC_SURCHARGE = 15.0;
    /// Threshold beyond which a car is classified as high-capacity (e.g. SUV, minivan)
    constexpr int CAR_LARGE_CAPACITY_THRESHOLD = 5;
    /// Daily surcharge for high-capacity vehicles (> 5 seats)
    constexpr double CAR_LARGE_CAPACITY_SURCHARGE = 20.0;
} // namespace pricing

/**
 * @brief Concrete derived class representing passenger cars, sedans, SUVs, and hatchbacks.
 */
class Car : public Vehicle {
private:
    int seatingCapacity{5};
    std::string fuelType;
    bool isAutomatic{true};

public:
    Car(int id,
        std::string brandName,
        std::string modelName,
        std::string plate,
        double dailyRate,
        int seats,
        std::string fuel,
        bool automatic,
        core::VehicleStatus initialStatus = core::VehicleStatus::Available);

    // Accessors
    int getSeatingCapacity() const noexcept { return seatingCapacity; }
    const std::string& getFuelType() const noexcept { return fuelType; }
    bool getIsAutomatic() const noexcept { return isAutomatic; }

    // Polymorphic overrides
    double calculateDailyRate() const override;
    std::string getVehicleType() const override;
    void displayDetails(std::ostream& os) const override;
};

} // namespace fleetflow::models
