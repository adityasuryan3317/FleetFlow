#pragma once

#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/DateRange.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/User.hpp"
#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/storage/Repository.hpp"

#include <memory>
#include <vector>

namespace fleetflow::services {

/**
 * @brief Application service for booking and managing rental lifecycles.
 *
 * RentalService coordinates the rental, vehicle, and user repositories. It
 * checks references and vehicle availability before booking, then delegates
 * lifecycle transitions to RentalRecord so its state-machine invariants remain
 * the single source of truth.
 *
 * The PaymentRecord ledger is deliberately not modified here: PaymentRecord
 * is immutable and payment processing is a separate service responsibility.
 * Pointers returned by this class are non-owning and follow Repository's
 * pointer-lifetime contract.
 */
class RentalService {
private:
    storage::Repository<models::RentalRecord>& rentalRepo;
    storage::Repository<models::Vehicle>& vehicleRepo;
    storage::Repository<models::User>& userRepo;

    models::RentalRecord& requireRental(int rentalId) const;

public:
    RentalService(storage::Repository<models::RentalRecord>& rRepo,
                  storage::Repository<models::Vehicle>& vRepo,
                  storage::Repository<models::User>& uRepo) noexcept;

    /**
     * @brief Books a rental after validating its customer, vehicle, cost, and availability.
     * @param rental Confirmed RentalRecord to insert. Ownership transfers on success.
     * @param today Reference date supplied to the RentalRecord conflict policy.
     * @throws core::ValidationException for a null rental or invalid estimated cost.
     * @throws core::EntityNotFoundException if the referenced vehicle or customer is absent.
     * @throws core::VehicleUnavailableException if the vehicle is not Available or dates conflict.
     * @throws core::DuplicateEntityException if the rental ID already exists.
     */
    models::RentalRecord* bookRental(std::unique_ptr<models::RentalRecord> rental,
                                     const core::Date& today);

    /** @brief Returns true only when the vehicle exists, is Available, and has no date conflict. */
    bool isVehicleAvailable(int vehicleId,
                            const core::DateRange& requestedRange,
                            const core::Date& today) const;

    // Rental lifecycle operations. State/date/cost rules are enforced by RentalRecord.
    void cancelRental(int rentalId);
    void activateRental(int rentalId, const core::Date& actualStartDate);
    void completeRental(int rentalId,
                        const core::Date& actualReturnDate,
                        double finalSettledCost);

    // Queries. getRental returns nullptr when the ID is absent.
    models::RentalRecord* getRental(int rentalId) const noexcept;
    std::vector<models::RentalRecord*> listRentals() const;
    std::vector<models::RentalRecord*> listRentalsForCustomer(int customerId) const;
    std::vector<models::RentalRecord*> listRentalsForVehicle(int vehicleId) const;
};

} // namespace fleetflow::services
