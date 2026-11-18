#pragma once

#include "fleetflow/models/Vehicle.hpp"

namespace fleetflow::models {

namespace pricing {
    /// Commercial wear and load insurance fee per ton of capacity ($/ton/day)
    constexpr double TRUCK_TONNAGE_RATE_PER_TON = 10.0;
    /// Baseline commercial axle count (standard 2-axle commercial truck)
    constexpr int TRUCK_BASE_AXLES = 2;
    /// Heavy-duty road tax and wear surcharge for axles beyond baseline ($/axle/day)
    constexpr double TRUCK_EXTRA_AXLE_SURCHARGE = 25.0;
} // namespace pricing

/**
 * @brief Concrete derived class representing commercial cargo haulers, flatbeds, and multi-axle trucks.
 */
class Truck : public Vehicle {
private:
    double cargoCapacityTons{2.5};
    int numberOfAxles{2};

public:
    Truck(int id,
          std::string brandName,
          std::string modelName,
          std::string plate,
          double dailyRate,
          double capacityTons,
          int axles,
          core::VehicleStatus initialStatus = core::VehicleStatus::Available);

    // Accessors
    double getCargoCapacityTons() const noexcept { return cargoCapacityTons; }
    int getNumberOfAxles() const noexcept { return numberOfAxles; }

    // Polymorphic overrides
    double calculateDailyRate() const override;
    std::string getVehicleType() const override;
    void displayDetails(std::ostream& os) const override;
};

} // namespace fleetflow::models
