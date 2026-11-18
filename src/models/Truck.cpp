#include "fleetflow/models/Truck.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include <iomanip>
#include <ostream>

namespace fleetflow::models {

Truck::Truck(int id,
             std::string brandName,
             std::string modelName,
             std::string plate,
             double dailyRate,
             double capacityTons,
             int axles,
             core::VehicleStatus initialStatus)
    : Vehicle(id, std::move(brandName), std::move(modelName), std::move(plate), dailyRate, initialStatus) {

    if (capacityTons <= 0.0) {
        throw core::ValidationException("Truck cargo capacity must be positive, received: " + std::to_string(capacityTons));
    }
    if (axles < 2) {
        throw core::ValidationException("Truck number of axles must be at least 2, received: " + std::to_string(axles));
    }

    cargoCapacityTons = capacityTons;
    numberOfAxles = axles;
}

double Truck::calculateDailyRate() const {
    double rate = getBaseDailyRate() + (cargoCapacityTons * pricing::TRUCK_TONNAGE_RATE_PER_TON);
    if (numberOfAxles > pricing::TRUCK_BASE_AXLES) {
        rate += (numberOfAxles - pricing::TRUCK_BASE_AXLES) * pricing::TRUCK_EXTRA_AXLE_SURCHARGE;
    }
    return rate;
}

std::string Truck::getVehicleType() const {
    return "Truck";
}

void Truck::displayDetails(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [TRUCK] " << getBrand() << " " << getModel() << " (ID: " << getVehicleId() << ")\n"
       << " License Plate    : " << getLicensePlate() << "\n"
       << " Operational Status: " << core::toString(getStatus()) << "\n"
       << " Cargo Capacity   : " << std::fixed << std::setprecision(1) << cargoCapacityTons << " tons\n"
       << " Axle Count       : " << numberOfAxles << " axles\n"
       << " Base Daily Rate  : ₹" << std::fixed << std::setprecision(2) << getBaseDailyRate() << "\n"
       << " Effective Rate   : ₹" << std::fixed << std::setprecision(2) << calculateDailyRate() << "/day\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

