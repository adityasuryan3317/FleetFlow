#pragma once

#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/models/User.hpp"
#include "fleetflow/storage/Repository.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"

#include <memory>
#include <vector>

namespace fleetflow::services {

/**
 * @brief Application service for fleet vehicles and system users.
 *
 * FleetService coordinates the corresponding repositories and enforces
 * referential integrity before a vehicle or user is deleted. The repositories
 * retain ownership of entities; pointers returned by this service are
 * non-owning and follow Repository's pointer-lifetime contract.
 */
class FleetService {
private:
    storage::Repository<models::Vehicle>& vehicleRepo;
    storage::Repository<models::User>& userRepo;
    storage::Repository<models::RentalRecord>& rentalRepo;
    storage::Repository<models::PaymentRecord>& paymentRepo;
    storage::Repository<models::MaintenanceRecord>& maintenanceRepo;

public:
    FleetService(storage::Repository<models::Vehicle>& vRepo,
                 storage::Repository<models::User>& uRepo,
                 storage::Repository<models::RentalRecord>& rRepo,
                 storage::Repository<models::PaymentRecord>& pRepo,
                 storage::Repository<models::MaintenanceRecord>& mRepo) noexcept;

    // Vehicle management
    models::Vehicle* addVehicle(std::unique_ptr<models::Vehicle> vehicle);
    void removeVehicle(int vehicleId);
    models::Vehicle* getVehicle(int vehicleId) const;
    std::vector<models::Vehicle*> listVehicles() const;

    // User management
    models::User* addUser(std::unique_ptr<models::User> user);
    void removeUser(int userId);
    models::User* getUser(int userId) const;
    std::vector<models::User*> listUsers() const;
};

} // namespace fleetflow::services
