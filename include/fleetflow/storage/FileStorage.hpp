#pragma once

#include "fleetflow/storage/Repository.hpp"
#include "fleetflow/models/Vehicle.hpp"
#include "fleetflow/models/User.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"

#include <filesystem>
#include <string>
#include <vector>
#include <cstddef>

namespace fleetflow::storage {

/**
 * @brief Diagnostic information for a record quarantined due to referential integrity failure.
 */
struct QuarantinedRecord {
    std::string datasetName;
    int recordId{0};
    std::string reason;
};

/**
 * @brief Comprehensive report of storage load and validation operations.
 */
struct StorageReport {
    std::size_t loadedVehicles{0};
    std::size_t loadedUsers{0};
    std::size_t loadedRentals{0};
    std::size_t loadedPayments{0};
    std::size_t loadedMaintenance{0};
    std::vector<std::string> syntaxWarnings;
    std::vector<QuarantinedRecord> quarantined;

    bool hasWarnings() const noexcept {
        return !syntaxWarnings.empty() || !quarantined.empty();
    }
};

/**
 * @brief Persistence coordinator managing CSV datasets with referential integrity and safe writes.
 *
 * Dataset Loading Order:
 * 1. Vehicles     (vehicles.csv)
 * 2. Users        (users.csv)
 * 3. Rentals      (rentals.csv)
 * 4. Payments     (payments.csv)
 * 5. Maintenance  (maintenance.csv)
 * 6. Referential Integrity Validation & Quarantine
 *
 * @note Multi-File Durability Contract:
 * Saving writes each dataset safely via a temporary file and backup-swap protocol.
 * Individual file writes are resilient against mid-write crashes, but saving across all
 * five files does not constitute a distributed multi-file atomic transaction.
 */
class FileStorage {
private:
    std::filesystem::path dataDirectory;

public:
    /**
     * @brief Constructs FileStorage with a configurable data directory.
     * @param dataDir Path to the directory holding CSV files (defaults to "data").
     */
    explicit FileStorage(std::filesystem::path dataDir = "data");

    // Accessors
    const std::filesystem::path& getDataDirectory() const noexcept { return dataDirectory; }
    void setDataDirectory(std::filesystem::path newDir) { dataDirectory = std::move(newDir); }

    /**
     * @brief Loads all 5 datasets in order, validates referential integrity, and commits results.
     *
     * Staging is used: if fatal filesystem I/O occurs, target repositories are not left
     * in a partially populated state. Records failing referential integrity are excluded
     * and quarantined in the returned report.
     *
     * @return StorageReport containing loaded counts, syntax warnings, and quarantined records.
     */
    StorageReport loadAll(
        Repository<models::Vehicle>& vehicleRepo,
        Repository<models::User>& userRepo,
        Repository<models::RentalRecord>& rentalRepo,
        Repository<models::PaymentRecord>& paymentRepo,
        Repository<models::MaintenanceRecord>& maintenanceRepo);

    /**
     * @brief Saves all 5 repositories to CSV datasets using the safe file replacement protocol.
     *
     * Records in each dataset are sorted deterministically by primary ID in ascending order.
     *
     * @warning Multi-File Atomicity Limitation:
     * Each individual dataset replacement is protected by the Backup-Swap protocol,
     * ensuring single-file durability and failure recovery. However, saving across all five
     * files does NOT constitute a distributed multi-file atomic transaction.
     * If saving succeeds for vehicles.csv, users.csv, and rentals.csv, but fails on
     * payments.csv:
     * - The first three files remain successfully updated on disk.
     * - payments.csv rolls back to its prior version (or remains unchanged).
     * - maintenance.csv is not reached and remains at its prior version.
     * - A core::StorageException is thrown identifying the payments dataset.
     */
    void saveAll(
        const Repository<models::Vehicle>& vehicleRepo,
        const Repository<models::User>& userRepo,
        const Repository<models::RentalRecord>& rentalRepo,
        const Repository<models::PaymentRecord>& paymentRepo,
        const Repository<models::MaintenanceRecord>& maintenanceRepo);

    // Individual dataset loaders (populates repo, records syntax/parse warnings)
    std::size_t loadVehicles(Repository<models::Vehicle>& repo, std::vector<std::string>& warnings);
    std::size_t loadUsers(Repository<models::User>& repo, std::vector<std::string>& warnings);
    std::size_t loadRentals(Repository<models::RentalRecord>& repo, std::vector<std::string>& warnings);
    std::size_t loadPayments(Repository<models::PaymentRecord>& repo, std::vector<std::string>& warnings);
    std::size_t loadMaintenance(Repository<models::MaintenanceRecord>& repo, std::vector<std::string>& warnings);

    // Individual dataset savers (writes sorted CSV with header)
    void saveVehicles(const Repository<models::Vehicle>& repo);
    void saveUsers(const Repository<models::User>& repo);
    void saveRentals(const Repository<models::RentalRecord>& repo);
    void savePayments(const Repository<models::PaymentRecord>& repo);
    void saveMaintenance(const Repository<models::MaintenanceRecord>& repo);

    /**
     * @brief Recovers dataset file in case of prior crash or interrupted replacement.
     *
     * Recovery rules:
     * - If target is missing and target.bak exists: restores target.bak -> target.
     * - If target exists but is 0 bytes (truncated) and target.bak is non-empty: restores target.bak -> target.
     * - If target exists and is valid, and target.bak exists: preserves target.bak (does not blindly delete).
     * - If target exists and target.tmp exists: cleans up dangling target.tmp.
     *
     * @param targetPath Destination dataset file path.
     */
    static void recoverDataset(const std::filesystem::path& targetPath);

    /**
     * @brief Safely replaces a target file using same-directory staging and backup-swap.
     *
     * If write/flush/close fails, the target file is untouched.
     * If promoting the temporary file fails, the original file is restored from backup.
     *
     * @param targetPath Path to destination file.
     * @param lines Content lines to write.
     * @throws core::StorageException on filesystem write/rename failure.
     */
    static void safeWriteFile(const std::filesystem::path& targetPath, const std::vector<std::string>& lines);
};

} // namespace fleetflow::storage

