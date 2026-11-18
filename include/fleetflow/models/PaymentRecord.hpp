#pragma once

#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Date.hpp"

#include <string>
#include <iosfwd>

namespace fleetflow::models {

/**
 * @brief Represents an immutable financial transaction in the FleetFlow ledger.
 * Records customer payments, advance deposits, settlement charges, and early return refunds.
 */
class PaymentRecord {
private:
    int paymentId{0};
    int rentalId{0};
    int customerId{0};
    double amount{0.0};
    core::Date transactionDate;
    core::PaymentType paymentType{core::PaymentType::Payment};
    core::PaymentMethod paymentMethod{core::PaymentMethod::Cash};
    std::string description;

public:
    PaymentRecord(int pId,
                  int rId,
                  int cId,
                  double amt,
                  core::Date transDate,
                  core::PaymentType pType,
                  core::PaymentMethod pMethod,
                  std::string desc);

    // Accessors
    int getPaymentId() const noexcept { return paymentId; }
    int getRentalId() const noexcept { return rentalId; }
    int getCustomerId() const noexcept { return customerId; }
    double getAmount() const noexcept { return amount; }
    const core::Date& getTransactionDate() const noexcept { return transactionDate; }
    core::PaymentType getPaymentType() const noexcept { return paymentType; }
    core::PaymentMethod getPaymentMethod() const noexcept { return paymentMethod; }
    const std::string& getDescription() const noexcept { return description; }

    void displayDetails(std::ostream& os) const;
};

} // namespace fleetflow::models

