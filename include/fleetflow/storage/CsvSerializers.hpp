#pragma once

#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/models/User.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"

#include <memory>
#include <string>
#include <vector>

namespace fleetflow::storage {

/**
 * @brief Dedicated serializer for polymorphic Vehicle objects (Car, Bike, Truck).
 *
 * Exact Schema (10 columns):
 * id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3
 */
class VehicleSerializer {
public:
    static std::vector<std::string> serialize(const models::Vehicle& vehicle);
    static std::unique_ptr<models::Vehicle> deserialize(const std::vector<std::string>& fields);
};

/**
 * @brief Dedicated serializer for polymorphic User objects (Customer, Employee, Admin).
 *
 * Exact Schema (7 columns):
 * id,role,username,fullName,email,extra1,extra2
 */
class UserSerializer {
public:
    static std::vector<std::string> serialize(const models::User& user);
    static std::unique_ptr<models::User> deserialize(const std::vector<std::string>& fields);
};

/**
 * @brief Dedicated serializer for RentalRecord objects.
 *
 * Exact Schema (10 columns):
 * rentalId,customerId,vehicleId,startDate,scheduledEndDate,actualReturnDate,estimatedCost,finalCost,status,actualStartDate
 */
class RentalSerializer {
public:
    static std::vector<std::string> serialize(const models::RentalRecord& record);
    static models::RentalRecord deserialize(const std::vector<std::string>& fields);
};

/**
 * @brief Dedicated serializer for PaymentRecord objects.
 *
 * Exact Schema (8 columns):
 * paymentId,rentalId,customerId,amount,date,type,method,description
 */
class PaymentSerializer {
public:
    static std::vector<std::string> serialize(const models::PaymentRecord& record);
    static models::PaymentRecord deserialize(const std::vector<std::string>& fields);
};

/**
 * @brief Dedicated serializer for MaintenanceRecord objects.
 *
 * Exact Schema (7 columns):
 * maintenanceId,vehicleId,problemDescription,startDate,completionDate,cost,status
 */
class MaintenanceSerializer {
public:
    static std::vector<std::string> serialize(const models::MaintenanceRecord& record);
    static models::MaintenanceRecord deserialize(const std::vector<std::string>& fields);
};

} // namespace fleetflow::storage

