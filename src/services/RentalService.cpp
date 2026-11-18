#include "fleetflow/services/RentalService.hpp"

#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/services/Validation.hpp"

#include <string>
#include <utility>

namespace fleetflow::services {

RentalService::RentalService(storage::Repository<models::RentalRecord>& rRepo,
                             storage::Repository<models::Vehicle>& vRepo,
                             storage::Repository<models::User>& uRepo) noexcept
    : rentalRepo(rRepo), vehicleRepo(vRepo), userRepo(uRepo) {}

models::RentalRecord& RentalService::requireRental(int rentalId) const {
    models::RentalRecord* rental = rentalRepo.getById(rentalId);
    if (rental == nullptr) {
        throw core::EntityNotFoundException(
            "Rental #" + std::to_string(rentalId) + " not found.");
    }
    return *rental;
}

models::RentalRecord* RentalService::bookRental(
    std::unique_ptr<models::RentalRecord> rental,
    const core::Date& today) {
    if (!rental) {
        throw core::ValidationException("Cannot book a null rental pointer.");
    }

    const int rentalId = rental->getRentalId();
    const int customerId = rental->getCustomerId();
    const int vehicleId = rental->getVehicleId();

    // Check duplicate identity before availability so repository identity errors
    // are deterministic and do not depend on the requested date window.
    if (rentalRepo.exists(rentalId)) {
        throw core::DuplicateEntityException(
            "Rental with ID #" + std::to_string(rentalId) + " already exists.");
    }

    if (userRepo.getById(customerId) == nullptr) {
        throw core::EntityNotFoundException(
            "Customer/user #" + std::to_string(customerId) + " not found.");
    }
    if (vehicleRepo.getById(vehicleId) == nullptr) {
        throw core::EntityNotFoundException(
            "Vehicle #" + std::to_string(vehicleId) + " not found.");
    }

    validateNonNegativeAmount(rental->getEstimatedCost(), "Estimated rental cost");

    if (!isVehicleAvailable(vehicleId, rental->getScheduledRange(), today)) {
        throw core::VehicleUnavailableException(
            "Vehicle #" + std::to_string(vehicleId) +
            " is not available for " + rental->getScheduledRange().toString() + ".");
    }

    rentalRepo.add(std::move(rental));
    return rentalRepo.getById(rentalId);
}

bool RentalService::isVehicleAvailable(
    int vehicleId,
    const core::DateRange& requestedRange,
    const core::Date& today) const {
    const models::Vehicle* vehicle = vehicleRepo.getById(vehicleId);
    if (vehicle == nullptr) {
        throw core::EntityNotFoundException(
            "Vehicle #" + std::to_string(vehicleId) + " not found.");
    }

    if (vehicle->getStatus() != core::VehicleStatus::Available) {
        return false;
    }

    for (const models::RentalRecord* existing : rentalRepo.getAll()) {
        if (existing->getVehicleId() == vehicleId &&
            existing->conflictsWith(requestedRange, today)) {
            return false;
        }
    }
    return true;
}

void RentalService::cancelRental(int rentalId) {
    requireRental(rentalId).markCancelled();
}

void RentalService::activateRental(int rentalId, const core::Date& actualStartDate) {
    requireRental(rentalId).markActive(actualStartDate);
}

void RentalService::completeRental(int rentalId,
                                   const core::Date& actualReturnDate,
                                   double finalSettledCost) {
    requireRental(rentalId).completeRental(actualReturnDate, finalSettledCost);
}

models::RentalRecord* RentalService::getRental(int rentalId) const noexcept {
    return rentalRepo.getById(rentalId);
}

std::vector<models::RentalRecord*> RentalService::listRentals() const {
    return rentalRepo.getAll();
}

std::vector<models::RentalRecord*> RentalService::listRentalsForCustomer(int customerId) const {
    std::vector<models::RentalRecord*> result;
    for (models::RentalRecord* rental : rentalRepo.getAll()) {
        if (rental->getCustomerId() == customerId) {
            result.push_back(rental);
        }
    }
    return result;
}

std::vector<models::RentalRecord*> RentalService::listRentalsForVehicle(int vehicleId) const {
    std::vector<models::RentalRecord*> result;
    for (models::RentalRecord* rental : rentalRepo.getAll()) {
        if (rental->getVehicleId() == vehicleId) {
            result.push_back(rental);
        }
    }
    return result;
}

} // namespace fleetflow::services
