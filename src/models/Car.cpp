#include "fleetflow/models/Car.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include <iomanip>
#include <ostream>

namespace fleetflow::models {

namespace {

bool isBlank(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

} // anonymous namespace

Car::Car(int id,
         std::string brandName,
         std::string modelName,
         std::string plate,
         double dailyRate,
         int seats,
         std::string fuel,
         bool automatic,
         core::VehicleStatus initialStatus)
    : Vehicle(id, std::move(brandName), std::move(modelName), std::move(plate), dailyRate, initialStatus) {

    if (seats < 1 || seats > 9) {
        throw core::ValidationException("Car seating capacity must be between 1 and 9, received: " + std::to_string(seats));
    }
    if (isBlank(fuel)) {
        throw core::ValidationException("Car fuel type cannot be empty or whitespace.");
    }

    seatingCapacity = seats;
    fuelType = std::move(fuel);
    isAutomatic = automatic;
}

double Car::calculateDailyRate() const {
    double rate = getBaseDailyRate();
    if (isAutomatic) {
        rate += pricing::CAR_AUTOMATIC_SURCHARGE;
    }
    if (seatingCapacity > pricing::CAR_LARGE_CAPACITY_THRESHOLD) {
        rate += pricing::CAR_LARGE_CAPACITY_SURCHARGE;
    }
    return rate;
}

std::string Car::getVehicleType() const {
    return "Car";
}

void Car::displayDetails(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [CAR] " << getBrand() << " " << getModel() << " (ID: " << getVehicleId() << ")\n"
       << " License Plate    : " << getLicensePlate() << "\n"
       << " Operational Status: " << core::toString(getStatus()) << "\n"
       << " Seating Capacity : " << seatingCapacity << " passengers\n"
       << " Fuel Type        : " << fuelType << "\n"
       << " Transmission     : " << (isAutomatic ? "Automatic" : "Manual") << "\n"
       << " Base Daily Rate  : ₹" << std::fixed << std::setprecision(2) << getBaseDailyRate() << "\n"
       << " Effective Rate   : ₹" << std::fixed << std::setprecision(2) << calculateDailyRate() << "/day\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

