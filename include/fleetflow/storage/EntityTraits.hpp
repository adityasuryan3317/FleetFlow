#pragma once

#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/models/User.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"

namespace fleetflow::storage {

/**
 * @brief Primary template for compile-time entity identifier extraction.
 * Specializations map each domain entity type to its specific ID accessor.
 */
template <typename T>
struct EntityTraits;

/**
 * @brief EntityTraits specialization for polymorphic Vehicle hierarchy.
 */
template <>
struct EntityTraits<models::Vehicle> {
    static int getId(const models::Vehicle& v) noexcept {
        return v.getVehicleId();
    }
};

/**
 * @brief EntityTraits specialization for polymorphic User hierarchy.
 */
template <>
struct EntityTraits<models::User> {
    static int getId(const models::User& u) noexcept {
        return u.getUserId();
    }
};

/**
 * @brief EntityTraits specialization for RentalRecord.
 */
template <>
struct EntityTraits<models::RentalRecord> {
    static int getId(const models::RentalRecord& r) noexcept {
        return r.getRentalId();
    }
};

/**
 * @brief EntityTraits specialization for PaymentRecord.
 */
template <>
struct EntityTraits<models::PaymentRecord> {
    static int getId(const models::PaymentRecord& p) noexcept {
        return p.getPaymentId();
    }
};

/**
 * @brief EntityTraits specialization for MaintenanceRecord.
 */
template <>
struct EntityTraits<models::MaintenanceRecord> {
    static int getId(const models::MaintenanceRecord& m) noexcept {
        return m.getMaintenanceId();
    }
};

} // namespace fleetflow::storage

