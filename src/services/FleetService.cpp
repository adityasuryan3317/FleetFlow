#include "fleetflow/services/FleetService.hpp"

#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/services/Exceptions.hpp"

#include <string>
#include <utility>

namespace fleetflow::services {

FleetService::FleetService(storage::Repository<models::Vehicle>& vRepo,
                           storage::Repository<models::User>& uRepo,
                           storage::Repository<models::RentalRecord>& rRepo,
                           storage::Repository<models::PaymentRecord>& pRepo,
                           storage::Repository<models::MaintenanceRecord>& mRepo) noexcept
    : vehicleRepo(vRepo),
      userRepo(uRepo),
      rentalRepo(rRepo),
      paymentRepo(pRepo),
      maintenanceRepo(mRepo) {}

models::Vehicle* FleetService::addVehicle(std::unique_ptr<models::Vehicle> vehicle) {
    if (!vehicle) {
        throw core::ValidationException("Cannot add a null vehicle pointer.");
    }

    const int vehicleId = vehicle->getVehicleId();
    // Repository enforces duplicate-ID validation and takes ownership on success.
    vehicleRepo.add(std::move(vehicle));
    return vehicleRepo.getById(vehicleId);
}

void FleetService::removeVehicle(int vehicleId) {
    if (!vehicleRepo.exists(vehicleId)) {
        throw core::EntityNotFoundException(
            "Vehicle #" + std::to_string(vehicleId) + " not found.");
    }

    for (const models::RentalRecord* rental : rentalRepo.getAll()) {
        if (rental->getVehicleId() == vehicleId) {
            throw BusinessException(
                "Cannot remove vehicle #" + std::to_string(vehicleId) +
                ": it is referenced by rental #" +
                std::to_string(rental->getRentalId()) + ".");
        }
    }

    for (const models::MaintenanceRecord* maintenance : maintenanceRepo.getAll()) {
        if (maintenance->getVehicleId() == vehicleId) {
            throw BusinessException(
                "Cannot remove vehicle #" + std::to_string(vehicleId) +
                ": it is referenced by maintenance record #" +
                std::to_string(maintenance->getMaintenanceId()) + ".");
        }
    }

    // The existence check above ensures this succeeds absent concurrent mutation.
    vehicleRepo.remove(vehicleId);
}

models::Vehicle* FleetService::getVehicle(int vehicleId) const {
    return vehicleRepo.getById(vehicleId);
}

std::vector<models::Vehicle*> FleetService::listVehicles() const {
    return vehicleRepo.getAll();
}

models::User* FleetService::addUser(std::unique_ptr<models::User> user) {
    if (!user) {
        throw core::ValidationException("Cannot add a null user pointer.");
    }

    const int userId = user->getUserId();
    // Repository enforces duplicate-ID validation and takes ownership on success.
    userRepo.add(std::move(user));
    return userRepo.getById(userId);
}

void FleetService::removeUser(int userId) {
    if (!userRepo.exists(userId)) {
        throw core::EntityNotFoundException(
            "User #" + std::to_string(userId) + " not found.");
    }

    for (const models::RentalRecord* rental : rentalRepo.getAll()) {
        if (rental->getCustomerId() == userId) {
            throw BusinessException(
                "Cannot remove user #" + std::to_string(userId) +
                ": the user is referenced by rental #" +
                std::to_string(rental->getRentalId()) + ".");
        }
    }

    for (const models::PaymentRecord* payment : paymentRepo.getAll()) {
        if (payment->getCustomerId() == userId) {
            throw BusinessException(
                "Cannot remove user #" + std::to_string(userId) +
                ": the user is referenced by payment #" +
                std::to_string(payment->getPaymentId()) + ".");
        }
    }

    userRepo.remove(userId);
}

models::User* FleetService::getUser(int userId) const {
    return userRepo.getById(userId);
}

std::vector<models::User*> FleetService::listUsers() const {
    return userRepo.getAll();
}

} // namespace fleetflow::services
