#include "fleetflow/models/Bike.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include <algorithm>
#include <iomanip>
#include <ostream>

namespace fleetflow::models {

Bike::Bike(int id,
           std::string brandName,
           std::string modelName,
           std::string plate,
           double dailyRate,
           int cc,
           bool geared,
           core::VehicleStatus initialStatus)
    : Vehicle(id, std::move(brandName), std::move(modelName), std::move(plate), dailyRate, initialStatus) {

    if (cc <= 0) {
        throw core::ValidationException("Bike engine displacement must be positive, received: " + std::to_string(cc));
    }

    engineCC = cc;
    hasGear = geared;
}

double Bike::calculateDailyRate() const {
    double rate = getBaseDailyRate();
    if (engineCC > pricing::BIKE_PREMIUM_CC_THRESHOLD) {
        rate += pricing::BIKE_PREMIUM_ENGINE_SURCHARGE;
    }
    if (!hasGear) {
        rate -= pricing::BIKE_SCOOTER_DISCOUNT;
    }
    return std::max(pricing::BIKE_MINIMUM_DAILY_RATE, rate);
}

std::string Bike::getVehicleType() const {
    return "Bike";
}

void Bike::displayDetails(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [BIKE] " << getBrand() << " " << getModel() << " (ID: " << getVehicleId() << ")\n"
       << " License Plate    : " << getLicensePlate() << "\n"
       << " Operational Status: " << core::toString(getStatus()) << "\n"
       << " Engine Capacity  : " << engineCC << " cc\n"
       << " Transmission     : " << (hasGear ? "Geared Motorcycle" : "Gearless Scooter") << "\n"
       << " Base Daily Rate  : ₹" << std::fixed << std::setprecision(2) << getBaseDailyRate() << "\n"
       << " Effective Rate   : ₹" << std::fixed << std::setprecision(2) << calculateDailyRate() << "/day\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

