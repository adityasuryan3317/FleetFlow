#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <iomanip>
#include <ostream>

namespace fleetflow::models {

namespace {

bool isBlank(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

} // anonymous namespace

PaymentRecord::PaymentRecord(int pId,
                             int rId,
                             int cId,
                             double amt,
                             core::Date transDate,
                             core::PaymentType pType,
                             core::PaymentMethod pMethod,
                             std::string desc)
    : transactionDate(transDate),
      paymentType(pType),
      paymentMethod(pMethod) {

    if (pId <= 0) {
        throw core::ValidationException("Payment ID must be a positive integer, received: " + std::to_string(pId));
    }
    if (rId <= 0) {
        throw core::ValidationException("Rental ID must be a positive integer, received: " + std::to_string(rId));
    }
    if (cId <= 0) {
        throw core::ValidationException("Customer ID must be a positive integer, received: " + std::to_string(cId));
    }
    if (amt <= 0.0) {
        throw core::ValidationException("Payment transaction amount must be positive, received: " + std::to_string(amt));
    }
    if (isBlank(desc)) {
        throw core::ValidationException("Payment transaction description cannot be empty or whitespace.");
    }

    paymentId = pId;
    rentalId = rId;
    customerId = cId;
    amount = amt;
    description = std::move(desc);
}

void PaymentRecord::displayDetails(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [TRANSACTION #" << paymentId << "]\n"
       << " Rental ID       : " << rentalId << "\n"
       << " Customer ID     : " << customerId << "\n"
       << " Date            : " << transactionDate.toString() << "\n"
       << " Transaction Type: " << core::toString(paymentType) << "\n"
       << " Settlement Mode : " << core::toString(paymentMethod) << "\n"
       << " Amount          : ₹" << std::fixed << std::setprecision(2) << amount << "\n"
       << " Description     : " << description << "\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

