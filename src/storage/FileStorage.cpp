#include "fleetflow/storage/FileStorage.hpp"
#include "fleetflow/storage/CsvHelper.hpp"
#include "fleetflow/storage/CsvSerializers.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fleetflow::storage {

namespace {

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

bool isHeaderLine(const std::string& line, const std::string& prefix) {
    std::string lower = toLower(line);
    return lower.rfind(toLower(prefix), 0) == 0;
}

bool isBlankLine(const std::string& line) {
    return line.find_first_not_of(" \t\r\n") == std::string::npos;
}

} // anonymous namespace

FileStorage::FileStorage(std::filesystem::path dataDir)
    : dataDirectory(std::move(dataDir)) {}

// ============================================================================
// Crash Recovery & Safe File Replacement Protocol
// ============================================================================

void FileStorage::recoverDataset(const std::filesystem::path& targetPath) {
    std::filesystem::path bakPath = targetPath;
    bakPath += ".bak";
    std::filesystem::path tmpPath = targetPath;
    tmpPath += ".tmp";

    std::error_code ec;
    auto targetStatus = std::filesystem::status(targetPath, ec);
    bool targetExists = !ec && std::filesystem::exists(targetStatus);

    ec.clear();
    auto bakStatus = std::filesystem::status(bakPath, ec);
    bool bakExists = !ec && std::filesystem::exists(bakStatus);

    ec.clear();
    auto tmpStatus = std::filesystem::status(tmpPath, ec);
    bool tmpExists = !ec && std::filesystem::exists(tmpStatus);

    // Scenario B: target missing, but backup exists
    // (Crash occurred after renaming target -> bak, before temp -> target)
    if (!targetExists && bakExists) {
        std::filesystem::rename(bakPath, targetPath, ec);
        if (ec) {
            throw core::StorageException("Crash recovery failed: unable to restore " +
                                         bakPath.string() + " to " + targetPath.string() + ": " + ec.message());
        }
        targetExists = true;
        bakExists = false;
    }
    // Scenario: target exists but was truncated to 0 bytes, while backup exists and is non-empty
    else if (targetExists && bakExists) {
        auto targetSize = std::filesystem::file_size(targetPath, ec);
        if (!ec && targetSize == 0) {
            auto bakSize = std::filesystem::file_size(bakPath, ec);
            if (!ec && bakSize > 0) {
                std::filesystem::remove(targetPath, ec);
                std::filesystem::rename(bakPath, targetPath, ec);
                if (ec) {
                    throw core::StorageException("Crash recovery failed: unable to restore non-empty backup " +
                                                 bakPath.string() + " over empty " + targetPath.string() + ": " + ec.message());
                }
            }
        }
        // If target is valid and non-empty, do NOT delete bakPath during recovery/startup!
        // Preserve recoverable data.
    }

    // Scenario A: Leftover unpromoted .tmp file
    if (tmpExists && (targetExists || bakExists)) {
        std::filesystem::remove(tmpPath, ec);
    }
}

void FileStorage::safeWriteFile(const std::filesystem::path& targetPath, const std::vector<std::string>& lines) {
    std::filesystem::path parentDir = targetPath.parent_path();
    if (!parentDir.empty()) {
        std::error_code dirEc;
        std::filesystem::create_directories(parentDir, dirEc);
        if (dirEc) {
            throw core::StorageException("Failed to create directory " + parentDir.string() + ": " + dirEc.message());
        }
    }

    // Recover any interrupted state before new write
    recoverDataset(targetPath);

    std::filesystem::path tempPath = targetPath;
    tempPath += ".tmp";
    std::filesystem::path bakPath = targetPath;
    bakPath += ".bak";

    // 1. Write dataset to temporary file in the exact same directory
    {
        std::ofstream ofs(tempPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!ofs.is_open()) {
            throw core::StorageException("Failed to open temporary file for writing " +
                                         targetPath.filename().string() + ": " + tempPath.string());
        }
        for (const auto& line : lines) {
            ofs << line << "\n";
        }
        ofs.flush();
        if (ofs.fail()) {
            ofs.close();
            std::error_code ec;
            std::filesystem::remove(tempPath, ec);
            throw core::StorageException("Failed to flush temporary file for " +
                                         targetPath.filename().string() + ": " + tempPath.string());
        }
        ofs.close();
        if (ofs.fail()) {
            std::error_code ec;
            std::filesystem::remove(tempPath, ec);
            throw core::StorageException("Failed to close temporary file cleanly for " +
                                         targetPath.filename().string() + ": " + tempPath.string());
        }
    }

    // 2. Backup-swap protocol
    std::error_code ec;
    bool targetExists = std::filesystem::exists(targetPath, ec);
    if (ec) {
        std::filesystem::remove(tempPath, ec);
        throw core::StorageException("Filesystem error inspecting target file " + targetPath.string() + ": " + ec.message());
    }

    if (targetExists) {
        // Remove prior backup if present to allow atomic replacement on Windows
        std::filesystem::remove(bakPath, ec);
        if (ec) {
            std::filesystem::remove(tempPath, ec);
            throw core::StorageException("Failed to remove prior backup file for " +
                                         targetPath.filename().string() + " (" + bakPath.string() + "): " + ec.message());
        }

        // Rename target -> backup
        std::filesystem::rename(targetPath, bakPath, ec);
        if (ec) {
            std::filesystem::remove(tempPath, ec);
            throw core::StorageException("Failed to backup original file for " +
                                         targetPath.filename().string() + " to " + bakPath.string() + ": " + ec.message());
        }

        // Rename temp -> target
        std::filesystem::rename(tempPath, targetPath, ec);
        if (ec) {
            // Rollback: restore backup to target
            std::error_code rbEc;
            std::filesystem::rename(bakPath, targetPath, rbEc);
            std::filesystem::remove(tempPath, rbEc);
            if (rbEc) {
                throw core::StorageException(
                    "Critical failure: failed to replace " + targetPath.filename().string() +
                    " AND failed to rollback from backup (" + bakPath.string() + "): " + rbEc.message());
            }
            throw core::StorageException(
                "Failed to replace target file with temporary file for " + targetPath.filename().string() +
                ". Original file preserved from backup. Error: " + ec.message());
        }

        // Successful promotion: remove backup
        std::filesystem::remove(bakPath, ec);
        if (ec) {
            throw core::StorageException("Failed to clean up backup file after successful replacement for " +
                                         targetPath.filename().string() + " (" + bakPath.string() + "): " + ec.message());
        }
    } else {
        // Target does not exist yet: simply promote temp -> target
        std::filesystem::rename(tempPath, targetPath, ec);
        if (ec) {
            std::filesystem::remove(tempPath, ec);
            throw core::StorageException("Failed to promote temporary file for " +
                                         targetPath.filename().string() + " to " + targetPath.string() + ": " + ec.message());
        }
    }
}

// ============================================================================
// Dataset Loaders
// ============================================================================

std::size_t FileStorage::loadVehicles(Repository<models::Vehicle>& repo, std::vector<std::string>& warnings) {
    const std::string datasetName = "vehicles.csv";
    std::filesystem::path path = dataDirectory / datasetName;

    recoverDataset(path);

    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory || status.type() == std::filesystem::file_type::not_found) {
            return 0; // Genuinely missing file handled gracefully
        }
        throw core::StorageException("Filesystem error inspecting " + datasetName + " (" + path.string() + "): " + ec.message());
    }
    if (!std::filesystem::exists(status)) {
        return 0; // Genuinely missing file handled gracefully
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw core::StorageException("Failed to open " + datasetName + " file for reading (permission denied or I/O error): " + path.string());
    }

    std::string line;
    std::size_t lineNum = 0;
    std::size_t loaded = 0;

    while (std::getline(ifs, line)) {
        ++lineNum;
        if (isBlankLine(line)) continue;
        if (lineNum == 1 && isHeaderLine(line, "id,")) continue;

        try {
            auto fields = CsvHelper::parseLine(line);
            auto vehicle = VehicleSerializer::deserialize(fields);
            int id = vehicle->getVehicleId();
            if (repo.exists(id)) {
                warnings.push_back(datasetName + ":line " + std::to_string(lineNum) +
                                   ": Duplicate Vehicle ID #" + std::to_string(id) + " ignored.");
                continue;
            }
            repo.add(vehicle);
            ++loaded;
        } catch (const std::exception& e) {
            warnings.push_back(datasetName + ":line " + std::to_string(lineNum) + ": " + e.what());
        }
    }

    if (ifs.bad()) {
        throw core::StorageException("Fatal I/O read failure while reading " + datasetName + " file: " + path.string());
    }

    return loaded;
}

std::size_t FileStorage::loadUsers(Repository<models::User>& repo, std::vector<std::string>& warnings) {
    const std::string datasetName = "users.csv";
    std::filesystem::path path = dataDirectory / datasetName;

    recoverDataset(path);

    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory || status.type() == std::filesystem::file_type::not_found) {
            return 0; // Genuinely missing file handled gracefully
        }
        throw core::StorageException("Filesystem error inspecting " + datasetName + " (" + path.string() + "): " + ec.message());
    }
    if (!std::filesystem::exists(status)) {
        return 0; // Genuinely missing file handled gracefully
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw core::StorageException("Failed to open " + datasetName + " file for reading (permission denied or I/O error): " + path.string());
    }

    std::string line;
    std::size_t lineNum = 0;
    std::size_t loaded = 0;

    while (std::getline(ifs, line)) {
        ++lineNum;
        if (isBlankLine(line)) continue;
        if (lineNum == 1 && isHeaderLine(line, "id,")) continue;

        try {
            auto fields = CsvHelper::parseLine(line);
            auto user = UserSerializer::deserialize(fields);
            int id = user->getUserId();
            if (repo.exists(id)) {
                warnings.push_back(datasetName + ":line " + std::to_string(lineNum) +
                                   ": Duplicate User ID #" + std::to_string(id) + " ignored.");
                continue;
            }
            repo.add(user);
            ++loaded;
        } catch (const std::exception& e) {
            warnings.push_back(datasetName + ":line " + std::to_string(lineNum) + ": " + e.what());
        }
    }

    if (ifs.bad()) {
        throw core::StorageException("Fatal I/O read failure while reading " + datasetName + " file: " + path.string());
    }

    return loaded;
}

std::size_t FileStorage::loadRentals(Repository<models::RentalRecord>& repo, std::vector<std::string>& warnings) {
    const std::string datasetName = "rentals.csv";
    std::filesystem::path path = dataDirectory / datasetName;

    recoverDataset(path);

    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory || status.type() == std::filesystem::file_type::not_found) {
            return 0; // Genuinely missing file handled gracefully
        }
        throw core::StorageException("Filesystem error inspecting " + datasetName + " (" + path.string() + "): " + ec.message());
    }
    if (!std::filesystem::exists(status)) {
        return 0; // Genuinely missing file handled gracefully
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw core::StorageException("Failed to open " + datasetName + " file for reading (permission denied or I/O error): " + path.string());
    }

    std::string line;
    std::size_t lineNum = 0;
    std::size_t loaded = 0;

    while (std::getline(ifs, line)) {
        ++lineNum;
        if (isBlankLine(line)) continue;
        if (lineNum == 1 && isHeaderLine(line, "rentalId")) continue;

        try {
            auto fields = CsvHelper::parseLine(line);
            auto rental = RentalSerializer::deserialize(fields);
            int id = rental.getRentalId();
            if (repo.exists(id)) {
                warnings.push_back(datasetName + ":line " + std::to_string(lineNum) +
                                   ": Duplicate Rental ID #" + std::to_string(id) + " ignored.");
                continue;
            }
            auto ptr = std::make_unique<models::RentalRecord>(std::move(rental));
            repo.add(ptr);
            ++loaded;
        } catch (const std::exception& e) {
            warnings.push_back(datasetName + ":line " + std::to_string(lineNum) + ": " + e.what());
        }
    }

    if (ifs.bad()) {
        throw core::StorageException("Fatal I/O read failure while reading " + datasetName + " file: " + path.string());
    }

    return loaded;
}

std::size_t FileStorage::loadPayments(Repository<models::PaymentRecord>& repo, std::vector<std::string>& warnings) {
    const std::string datasetName = "payments.csv";
    std::filesystem::path path = dataDirectory / datasetName;

    recoverDataset(path);

    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory || status.type() == std::filesystem::file_type::not_found) {
            return 0; // Genuinely missing file handled gracefully
        }
        throw core::StorageException("Filesystem error inspecting " + datasetName + " (" + path.string() + "): " + ec.message());
    }
    if (!std::filesystem::exists(status)) {
        return 0; // Genuinely missing file handled gracefully
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw core::StorageException("Failed to open " + datasetName + " file for reading (permission denied or I/O error): " + path.string());
    }

    std::string line;
    std::size_t lineNum = 0;
    std::size_t loaded = 0;

    while (std::getline(ifs, line)) {
        ++lineNum;
        if (isBlankLine(line)) continue;
        if (lineNum == 1 && isHeaderLine(line, "paymentId")) continue;

        try {
            auto fields = CsvHelper::parseLine(line);
            auto payment = PaymentSerializer::deserialize(fields);
            int id = payment.getPaymentId();
            if (repo.exists(id)) {
                warnings.push_back(datasetName + ":line " + std::to_string(lineNum) +
                                   ": Duplicate Payment ID #" + std::to_string(id) + " ignored.");
                continue;
            }
            auto ptr = std::make_unique<models::PaymentRecord>(std::move(payment));
            repo.add(ptr);
            ++loaded;
        } catch (const std::exception& e) {
            warnings.push_back(datasetName + ":line " + std::to_string(lineNum) + ": " + e.what());
        }
    }

    if (ifs.bad()) {
        throw core::StorageException("Fatal I/O read failure while reading " + datasetName + " file: " + path.string());
    }

    return loaded;
}

std::size_t FileStorage::loadMaintenance(Repository<models::MaintenanceRecord>& repo, std::vector<std::string>& warnings) {
    const std::string datasetName = "maintenance.csv";
    std::filesystem::path path = dataDirectory / datasetName;

    recoverDataset(path);

    std::error_code ec;
    auto status = std::filesystem::status(path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory || status.type() == std::filesystem::file_type::not_found) {
            return 0; // Genuinely missing file handled gracefully
        }
        throw core::StorageException("Filesystem error inspecting " + datasetName + " (" + path.string() + "): " + ec.message());
    }
    if (!std::filesystem::exists(status)) {
        return 0; // Genuinely missing file handled gracefully
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw core::StorageException("Failed to open " + datasetName + " file for reading (permission denied or I/O error): " + path.string());
    }

    std::string line;
    std::size_t lineNum = 0;
    std::size_t loaded = 0;

    while (std::getline(ifs, line)) {
        ++lineNum;
        if (isBlankLine(line)) continue;
        if (lineNum == 1 && isHeaderLine(line, "maintenanceId")) continue;

        try {
            auto fields = CsvHelper::parseLine(line);
            auto maint = MaintenanceSerializer::deserialize(fields);
            int id = maint.getMaintenanceId();
            if (repo.exists(id)) {
                warnings.push_back(datasetName + ":line " + std::to_string(lineNum) +
                                   ": Duplicate Maintenance ID #" + std::to_string(id) + " ignored.");
                continue;
            }
            auto ptr = std::make_unique<models::MaintenanceRecord>(std::move(maint));
            repo.add(ptr);
            ++loaded;
        } catch (const std::exception& e) {
            warnings.push_back(datasetName + ":line " + std::to_string(lineNum) + ": " + e.what());
        }
    }

    if (ifs.bad()) {
        throw core::StorageException("Fatal I/O read failure while reading " + datasetName + " file: " + path.string());
    }

    return loaded;
}

// ============================================================================
// Comprehensive Loading & Referential Integrity Validation Pipeline
// ============================================================================

StorageReport FileStorage::loadAll(
    Repository<models::Vehicle>& vehicleRepo,
    Repository<models::User>& userRepo,
    Repository<models::RentalRecord>& rentalRepo,
    Repository<models::PaymentRecord>& paymentRepo,
    Repository<models::MaintenanceRecord>& maintenanceRepo) {

    StorageReport report;

    // Staging repositories to guarantee all-or-nothing atomicity on fatal errors
    Repository<models::Vehicle> stagedVehicles;
    Repository<models::User> stagedUsers;
    Repository<models::RentalRecord> stagedRentals;
    Repository<models::PaymentRecord> stagedPayments;
    Repository<models::MaintenanceRecord> stagedMaintenance;

    // 1. Ingest datasets in strict sequence
    loadVehicles(stagedVehicles, report.syntaxWarnings);
    loadUsers(stagedUsers, report.syntaxWarnings);
    loadRentals(stagedRentals, report.syntaxWarnings);
    loadPayments(stagedPayments, report.syntaxWarnings);
    loadMaintenance(stagedMaintenance, report.syntaxWarnings);

    // 2. Referential Integrity Validation & Quarantine

    // Validate Rentals
    std::vector<int> invalidRentalIds;
    for (const auto* rental : stagedRentals.getAll()) {
        int rId = rental->getRentalId();
        int cId = rental->getCustomerId();
        int vId = rental->getVehicleId();

        const auto* user = stagedUsers.getById(cId);
        if (!user) {
            report.quarantined.push_back({"rentals.csv", rId,
                "Referenced Customer ID #" + std::to_string(cId) + " does not exist."});
            invalidRentalIds.push_back(rId);
            continue;
        }
        if (dynamic_cast<const models::Customer*>(user) == nullptr) {
            report.quarantined.push_back({"rentals.csv", rId,
                "Referenced User ID #" + std::to_string(cId) + " is not a registered Customer (" + user->getRoleName() + ")."});
            invalidRentalIds.push_back(rId);
            continue;
        }
        if (!stagedVehicles.exists(vId)) {
            report.quarantined.push_back({"rentals.csv", rId,
                "Referenced Vehicle ID #" + std::to_string(vId) + " does not exist."});
            invalidRentalIds.push_back(rId);
            continue;
        }
    }
    for (int id : invalidRentalIds) {
        stagedRentals.remove(id);
    }

    // Validate Payments
    std::vector<int> invalidPaymentIds;
    for (const auto* payment : stagedPayments.getAll()) {
        int pId = payment->getPaymentId();
        int rId = payment->getRentalId();
        int cId = payment->getCustomerId();

        const auto* rental = stagedRentals.getById(rId);
        if (!rental) {
            report.quarantined.push_back({"payments.csv", pId,
                "Referenced RentalRecord ID #" + std::to_string(rId) + " does not exist or was quarantined."});
            invalidPaymentIds.push_back(pId);
            continue;
        }

        const auto* user = stagedUsers.getById(cId);
        if (!user || dynamic_cast<const models::Customer*>(user) == nullptr) {
            report.quarantined.push_back({"payments.csv", pId,
                "Referenced Customer ID #" + std::to_string(cId) + " does not exist or is not a Customer."});
            invalidPaymentIds.push_back(pId);
            continue;
        }

        if (cId != rental->getCustomerId()) {
            report.quarantined.push_back({"payments.csv", pId,
                "Payment customer ID #" + std::to_string(cId) +
                " does not match Rental customer ID #" + std::to_string(rental->getCustomerId()) + "."});
            invalidPaymentIds.push_back(pId);
            continue;
        }
    }
    for (int id : invalidPaymentIds) {
        stagedPayments.remove(id);
    }

    // Validate Maintenance
    std::vector<int> invalidMaintenanceIds;
    for (const auto* maint : stagedMaintenance.getAll()) {
        int mId = maint->getMaintenanceId();
        int vId = maint->getVehicleId();

        if (!stagedVehicles.exists(vId)) {
            report.quarantined.push_back({"maintenance.csv", mId,
                "Referenced Vehicle ID #" + std::to_string(vId) + " does not exist."});
            invalidMaintenanceIds.push_back(mId);
            continue;
        }
    }
    for (int id : invalidMaintenanceIds) {
        stagedMaintenance.remove(id);
    }

    // 3. Commit staged, verified repositories
    vehicleRepo = std::move(stagedVehicles);
    userRepo = std::move(stagedUsers);
    rentalRepo = std::move(stagedRentals);
    paymentRepo = std::move(stagedPayments);
    maintenanceRepo = std::move(stagedMaintenance);

    report.loadedVehicles = vehicleRepo.size();
    report.loadedUsers = userRepo.size();
    report.loadedRentals = rentalRepo.size();
    report.loadedPayments = paymentRepo.size();
    report.loadedMaintenance = maintenanceRepo.size();

    return report;
}

// ============================================================================
// Dataset Savers (Deterministic Ordering)
// ============================================================================

void FileStorage::saveVehicles(const Repository<models::Vehicle>& repo) {
    auto items = repo.getAll();
    std::sort(items.begin(), items.end(), [](const auto* a, const auto* b) {
        return a->getVehicleId() < b->getVehicleId();
    });

    std::vector<std::string> lines;
    lines.reserve(items.size() + 1);
    lines.push_back("id,type,brand,model,plate,baseRate,status,extra1,extra2,extra3");

    for (const auto* v : items) {
        lines.push_back(CsvHelper::escapeRow(VehicleSerializer::serialize(*v)));
    }
    safeWriteFile(dataDirectory / "vehicles.csv", lines);
}

void FileStorage::saveUsers(const Repository<models::User>& repo) {
    auto items = repo.getAll();
    std::sort(items.begin(), items.end(), [](const auto* a, const auto* b) {
        return a->getUserId() < b->getUserId();
    });

    std::vector<std::string> lines;
    lines.reserve(items.size() + 1);
    lines.push_back("id,role,username,fullName,email,extra1,extra2");

    for (const auto* u : items) {
        lines.push_back(CsvHelper::escapeRow(UserSerializer::serialize(*u)));
    }
    safeWriteFile(dataDirectory / "users.csv", lines);
}

void FileStorage::saveRentals(const Repository<models::RentalRecord>& repo) {
    auto items = repo.getAll();
    std::sort(items.begin(), items.end(), [](const auto* a, const auto* b) {
        return a->getRentalId() < b->getRentalId();
    });

    std::vector<std::string> lines;
    lines.reserve(items.size() + 1);
    lines.push_back("rentalId,customerId,vehicleId,startDate,scheduledEndDate,actualReturnDate,estimatedCost,finalCost,status,actualStartDate");

    for (const auto* r : items) {
        lines.push_back(CsvHelper::escapeRow(RentalSerializer::serialize(*r)));
    }
    safeWriteFile(dataDirectory / "rentals.csv", lines);
}

void FileStorage::savePayments(const Repository<models::PaymentRecord>& repo) {
    auto items = repo.getAll();
    std::sort(items.begin(), items.end(), [](const auto* a, const auto* b) {
        return a->getPaymentId() < b->getPaymentId();
    });

    std::vector<std::string> lines;
    lines.reserve(items.size() + 1);
    lines.push_back("paymentId,rentalId,customerId,amount,date,type,method,description");

    for (const auto* p : items) {
        lines.push_back(CsvHelper::escapeRow(PaymentSerializer::serialize(*p)));
    }
    safeWriteFile(dataDirectory / "payments.csv", lines);
}

void FileStorage::saveMaintenance(const Repository<models::MaintenanceRecord>& repo) {
    auto items = repo.getAll();
    std::sort(items.begin(), items.end(), [](const auto* a, const auto* b) {
        return a->getMaintenanceId() < b->getMaintenanceId();
    });

    std::vector<std::string> lines;
    lines.reserve(items.size() + 1);
    lines.push_back("maintenanceId,vehicleId,problemDescription,startDate,completionDate,cost,status");

    for (const auto* m : items) {
        lines.push_back(CsvHelper::escapeRow(MaintenanceSerializer::serialize(*m)));
    }
    safeWriteFile(dataDirectory / "maintenance.csv", lines);
}

void FileStorage::saveAll(
    const Repository<models::Vehicle>& vehicleRepo,
    const Repository<models::User>& userRepo,
    const Repository<models::RentalRecord>& rentalRepo,
    const Repository<models::PaymentRecord>& paymentRepo,
    const Repository<models::MaintenanceRecord>& maintenanceRepo) {

    saveVehicles(vehicleRepo);
    saveUsers(userRepo);
    saveRentals(rentalRepo);
    savePayments(paymentRepo);
    saveMaintenance(maintenanceRepo);
}

} // namespace fleetflow::storage

