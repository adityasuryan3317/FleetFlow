#pragma once

#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/User.hpp"
#include "fleetflow/storage/Repository.hpp"

#include <memory>
#include <vector>

namespace fleetflow::services {

/**
 * @brief Coordinates immutable payment/refund ledger transactions.
 *
 * PaymentService validates that every transaction references an existing rental
 * and user, and that the transaction's customer matches the rental's customer.
 * PaymentRecord remains immutable; corrections are represented by additional
 * ledger entries (for example, a Refund), never by editing or deleting an entry.
 *
 * This service reports net recorded transactions, treating Payment as positive
 * and Refund as negative. It does not impose an unprovided settlement/refund cap.
 * Returned pointers are non-owning and follow Repository's pointer-lifetime
 * contract.
 */
class PaymentService {
private:
    storage::Repository<models::PaymentRecord>& paymentRepo;
    storage::Repository<models::RentalRecord>& rentalRepo;
    storage::Repository<models::User>& userRepo;

    models::RentalRecord& requireRental(int rentalId) const;
    models::User& requireCustomer(int customerId) const;

public:
    PaymentService(storage::Repository<models::PaymentRecord>& pRepo,
                   storage::Repository<models::RentalRecord>& rRepo,
                   storage::Repository<models::User>& uRepo) noexcept;

    /**
     * @brief Records an immutable payment or refund transaction.
     * @param transaction Transaction to insert; ownership transfers on success.
     * @throws core::ValidationException for null, invalid amount/enums, or a
     *         customer ID inconsistent with the referenced rental.
     * @throws core::EntityNotFoundException if its rental or customer is absent.
     * @throws core::DuplicateEntityException if the payment ID already exists.
     */
    models::PaymentRecord* recordTransaction(
        std::unique_ptr<models::PaymentRecord> transaction);

    // Direct lookup returns nullptr when absent.
    models::PaymentRecord* getTransaction(int paymentId) const noexcept;

    // Query operations. The referenced rental/customer must exist.
    std::vector<models::PaymentRecord*> listTransactions() const;
    std::vector<models::PaymentRecord*> listTransactionsForRental(int rentalId) const;
    std::vector<models::PaymentRecord*> listTransactionsForCustomer(int customerId) const;

    /**
     * @brief Returns the net recorded amount for a rental (payments minus refunds).
     * @throws core::EntityNotFoundException if the rental does not exist.
     */
    double getNetAmountForRental(int rentalId) const;

    /**
     * @brief Returns the net recorded amount across all rentals for a customer.
     * @throws core::EntityNotFoundException if the customer does not exist.
     */
    double getNetAmountForCustomer(int customerId) const;
};

} // namespace fleetflow::services
