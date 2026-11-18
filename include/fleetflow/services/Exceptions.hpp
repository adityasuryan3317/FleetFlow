#pragma once

#include <string>

#include "fleetflow/core/Exceptions.hpp"

namespace fleetflow::services {

/**
 * @brief Exception type for service‑level business‑rule violations.
 *
 * Derives from the core root exception (fleetflow::core::FleetFlowException) to keep
 * the existing exception hierarchy intact. The constructor prefixes the message
 * with a clear hint that the error originates from the service layer.
 */
class BusinessException : public fleetflow::core::FleetFlowException {
public:
    explicit BusinessException(const std::string& message)
        : fleetflow::core::FleetFlowException("Business Error: " + message) {}
};

} // namespace fleetflow::services

