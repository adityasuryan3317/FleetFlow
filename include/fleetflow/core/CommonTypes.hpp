#pragma once

#include <string>
#include <stdexcept>

namespace fleetflow::core {

/**
 * @brief Operational and road-readiness status of a vehicle.
 * @note This does NOT indicate whether a vehicle is currently reserved.
 * Availability for booking is computed dynamically against rental records.
 */
enum class VehicleStatus {
    Available,        ///< Mechanically sound, road-ready, eligible for booking
    UnderMaintenance, ///< Currently undergoing inspection, service, or repairs
    Retired           ///< Decommissioned or permanently removed from active fleet
};

/**
 * @brief Lifecycle status of a rental booking reservation.
 */
enum class RentalStatus {
    Confirmed, ///< Reserved in advance; not yet checked out
    Active,    ///< Vehicle picked up by customer and currently on the road
    Completed, ///< Vehicle returned, final billing settled
    Cancelled  ///< Reservation cancelled; date holds released
};

/**
 * @brief System user roles for Role-Based Access Control (RBAC).
 */
enum class UserRole {
    Customer, ///< Can browse available vehicles, book, and view personal rentals
    Employee, ///< Can check out/in vehicles, inspect returns, and manage customers
    Admin     ///< Full system control: fleet management, financial ledgers, reports
};

/**
 * @brief Type of financial ledger transaction.
 */
enum class PaymentType {
    Payment, ///< Customer pays company (advance deposit, rental balance, penalties)
    Refund   ///< Company refunds customer (early return balance, deposit return)
};

/**
 * @brief Accepted payment settlement methods.
 */
enum class PaymentMethod {
    Cash,
    Card,
    UPI
};

/**
 * @brief Servicing lifecycle status of a vehicle maintenance record.
 */
enum class MaintenanceStatus {
    Scheduled,  ///< Maintenance scheduled but vehicle not yet in workshop
    InProgress, ///< Vehicle currently undergoing inspection, repairs, or service
    Completed   ///< Servicing complete, inspection passed, vehicle cleared
};

// ============================================================================
// String Conversion Helpers
// ============================================================================

inline std::string toString(VehicleStatus status) {
    switch (status) {
        case VehicleStatus::Available:        return "Available";
        case VehicleStatus::UnderMaintenance: return "UnderMaintenance";
        case VehicleStatus::Retired:          return "Retired";
    }
    return "Unknown";
}

inline VehicleStatus vehicleStatusFromString(const std::string& str) {
    if (str == "Available")        return VehicleStatus::Available;
    if (str == "UnderMaintenance") return VehicleStatus::UnderMaintenance;
    if (str == "Retired")          return VehicleStatus::Retired;
    throw std::invalid_argument("Unknown VehicleStatus string: " + str);
}

inline std::string toString(RentalStatus status) {
    switch (status) {
        case RentalStatus::Confirmed: return "Confirmed";
        case RentalStatus::Active:    return "Active";
        case RentalStatus::Completed: return "Completed";
        case RentalStatus::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

inline RentalStatus rentalStatusFromString(const std::string& str) {
    if (str == "Confirmed") return RentalStatus::Confirmed;
    if (str == "Active")    return RentalStatus::Active;
    if (str == "Completed") return RentalStatus::Completed;
    if (str == "Cancelled") return RentalStatus::Cancelled;
    throw std::invalid_argument("Unknown RentalStatus string: " + str);
}

inline std::string toString(UserRole role) {
    switch (role) {
        case UserRole::Customer: return "Customer";
        case UserRole::Employee: return "Employee";
        case UserRole::Admin:    return "Admin";
    }
    return "Unknown";
}

inline UserRole userRoleFromString(const std::string& str) {
    if (str == "Customer") return UserRole::Customer;
    if (str == "Employee") return UserRole::Employee;
    if (str == "Admin")    return UserRole::Admin;
    throw std::invalid_argument("Unknown UserRole string: " + str);
}

inline std::string toString(PaymentType type) {
    switch (type) {
        case PaymentType::Payment: return "Payment";
        case PaymentType::Refund:  return "Refund";
    }
    return "Unknown";
}

inline PaymentType paymentTypeFromString(const std::string& str) {
    if (str == "Payment") return PaymentType::Payment;
    if (str == "Refund")  return PaymentType::Refund;
    throw std::invalid_argument("Unknown PaymentType string: " + str);
}

inline std::string toString(PaymentMethod method) {
    switch (method) {
        case PaymentMethod::Cash: return "Cash";
        case PaymentMethod::Card: return "Card";
        case PaymentMethod::UPI:  return "UPI";
    }
    return "Unknown";
}

inline PaymentMethod paymentMethodFromString(const std::string& str) {
    if (str == "Cash") return PaymentMethod::Cash;
    if (str == "Card") return PaymentMethod::Card;
    if (str == "UPI")  return PaymentMethod::UPI;
    throw std::invalid_argument("Unknown PaymentMethod string: " + str);
}

inline std::string toString(MaintenanceStatus status) {
    switch (status) {
        case MaintenanceStatus::Scheduled:  return "Scheduled";
        case MaintenanceStatus::InProgress: return "InProgress";
        case MaintenanceStatus::Completed:  return "Completed";
    }
    return "Unknown";
}

inline MaintenanceStatus maintenanceStatusFromString(const std::string& str) {
    if (str == "Scheduled")  return MaintenanceStatus::Scheduled;
    if (str == "InProgress") return MaintenanceStatus::InProgress;
    if (str == "Completed")  return MaintenanceStatus::Completed;
    throw std::invalid_argument("Unknown MaintenanceStatus string: " + str);
}

} // namespace fleetflow::core

