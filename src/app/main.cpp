#include "fleetflow/core/Date.hpp"
#include "fleetflow/core/DateRange.hpp"
#include "fleetflow/core/CommonTypes.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include "fleetflow/models/Admin.hpp"
#include "fleetflow/models/Bike.hpp"
#include "fleetflow/models/Car.hpp"
#include "fleetflow/models/Customer.hpp"
#include "fleetflow/models/Employee.hpp"
#include "fleetflow/models/MaintenanceRecord.hpp"
#include "fleetflow/models/PaymentRecord.hpp"
#include "fleetflow/models/RentalRecord.hpp"
#include "fleetflow/models/Truck.hpp"
#include "fleetflow/services/FleetService.hpp"
#include "fleetflow/services/MaintenanceService.hpp"
#include "fleetflow/services/PaymentService.hpp"
#include "fleetflow/services/RentalService.hpp"
#include "fleetflow/storage/FileStorage.hpp"
#include "fleetflow/storage/Repository.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using fleetflow::core::Date;
using fleetflow::core::DateRange;
using fleetflow::models::Admin;
using fleetflow::models::Bike;
using fleetflow::models::Car;
using fleetflow::models::Customer;
using fleetflow::models::Employee;
using fleetflow::models::MaintenanceRecord;
using fleetflow::models::PaymentRecord;
using fleetflow::models::RentalRecord;
using fleetflow::models::Truck;
using fleetflow::models::User;
using fleetflow::models::Vehicle;
using fleetflow::services::FleetService;
using fleetflow::services::MaintenanceService;
using fleetflow::services::PaymentService;
using fleetflow::services::RentalService;
using fleetflow::storage::FileStorage;
using fleetflow::storage::Repository;

struct AppContext {
    Repository<Vehicle> vehicles;
    Repository<User> users;
    Repository<RentalRecord> rentals;
    Repository<PaymentRecord> payments;
    Repository<MaintenanceRecord> maintenanceRecords;

    FileStorage storage{"data"};
    FleetService fleet{vehicles, users, rentals, payments, maintenanceRecords};
    RentalService rentalService{rentals, vehicles, users};
    PaymentService paymentService{payments, rentals, users};
    MaintenanceService maintenanceService{maintenanceRecords, vehicles, rentals};
};

bool readLine(const std::string& prompt, std::string& value) {
    std::cout << prompt;
    return static_cast<bool>(std::getline(std::cin, value));
}

bool readText(const std::string& prompt, std::string& value) {
    for (;;) {
        if (!readLine(prompt, value)) return false;
        if (value.find_first_not_of(" \t\r\n") != std::string::npos) return true;
        std::cout << "Please enter a non-empty value.\n";
    }
}

bool readInt(const std::string& prompt, int& value) {
    for (;;) {
        std::string line;
        if (!readLine(prompt, line)) return false;
        errno = 0;
        char* end = nullptr;
        const long parsed = std::strtol(line.c_str(), &end, 10);
        while (end && (*end == ' ' || *end == '\t' || *end == '\r')) ++end;
        if (errno == 0 && end != line.c_str() && end && *end == '\0' &&
            parsed >= std::numeric_limits<int>::min() &&
            parsed <= std::numeric_limits<int>::max()) {
            value = static_cast<int>(parsed);
            return true;
        }
        std::cout << "Invalid integer. Please try again.\n";
    }
}

bool readDouble(const std::string& prompt, double& value) {
    for (;;) {
        std::string line;
        if (!readLine(prompt, line)) return false;
        errno = 0;
        char* end = nullptr;
        const double parsed = std::strtod(line.c_str(), &end);
        while (end && (*end == ' ' || *end == '\t' || *end == '\r')) ++end;
        if (errno == 0 && end != line.c_str() && end && *end == '\0' && std::isfinite(parsed)) {
            value = parsed;
            return true;
        }
        std::cout << "Invalid number. Enter a finite numeric value.\n";
    }
}

bool readDate(const std::string& prompt, Date& value) {
    for (;;) {
        std::string line;
        if (!readLine(prompt + " (YYYY-MM-DD): ", line)) return false;
        try {
            value = Date::fromString(line);
            return true;
        } catch (const std::exception& error) {
            std::cout << error.what() << '\n';
        }
    }
}

Date currentLocalDate() {
    const std::time_t now = std::time(nullptr);
    const std::tm* local = std::localtime(&now);
    if (!local) {
        throw std::runtime_error("Unable to determine the current local date.");
    }
    return Date(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
}

void printHeader() {
    std::cout << "\n"
              << "============================================================\n"
              << "                    FLEETFLOW MANAGER                       \n"
              << "             Fleet, Rental & Operations Console             \n"
              << "============================================================\n";
}

void printMenu() {
    std::cout << "\nMAIN MENU\n"
              << "  1. Dashboard / summary\n"
              << "  2. List vehicles\n"
              << "  3. Add vehicle\n"
              << "  4. Remove vehicle\n"
              << "  5. List users\n"
              << "  6. Add user\n"
              << "  7. Remove user\n"
              << "  8. Book rental\n"
              << "  9. List rentals\n"
              << " 10. Cancel rental\n"
              << " 11. Activate rental (vehicle pickup)\n"
              << " 12. Complete rental (vehicle return)\n"
              << " 13. Record payment / refund\n"
              << " 14. List payment transactions\n"
              << " 15. Show net amount for rental\n"
              << " 16. Schedule maintenance\n"
              << " 17. List maintenance records\n"
              << " 18. Start maintenance\n"
              << " 19. Complete maintenance\n"
              << " 20. Save all data now\n"
              << "  0. Save and exit\n";
}

void saveAll(AppContext& app) {
    app.storage.saveAll(app.vehicles, app.users, app.rentals,
                        app.payments, app.maintenanceRecords);
    std::cout << "Data saved to: " << app.storage.getDataDirectory().string() << '\n';
}

void saveAfterMutation(AppContext& app) {
    try {
        saveAll(app);
    } catch (const std::exception& error) {
        std::cout << "WARNING: The operation changed in-memory data, but saving failed: "
                  << error.what() << '\n';
        std::cout << "Use option 20 to retry saving after resolving the storage issue.\n";
    }
}

void showDashboard(const AppContext& app) {
    std::cout << "\n---------------- FLEETFLOW SUMMARY ----------------\n"
              << "Vehicles            : " << app.vehicles.size() << '\n'
              << "Users               : " << app.users.size() << '\n'
              << "Rental bookings     : " << app.rentals.size() << '\n'
              << "Payment transactions: " << app.payments.size() << '\n'
              << "Maintenance records : " << app.maintenanceRecords.size() << '\n'
              << "---------------------------------------------------\n";
}

void listVehicles(const AppContext& app) {
    auto items = app.fleet.listVehicles();
    std::sort(items.begin(), items.end(), [](const Vehicle* lhs, const Vehicle* rhs) {
        return lhs->getVehicleId() < rhs->getVehicleId();
    });
    if (items.empty()) {
        std::cout << "No vehicles are registered yet.\n";
        return;
    }
    for (const Vehicle* vehicle : items) vehicle->displayDetails(std::cout);
}

bool addVehicle(AppContext& app) {
    std::cout << "\nVEHICLE TYPE\n  1. Car\n  2. Bike\n  3. Truck\n";
    int type = 0;
    if (!readInt("Select type: ", type)) return false;

    int id = 0;
    double baseRate = 0.0;
    std::string brand, model, plate;
    if (!readInt("Vehicle ID: ", id) ||
        !readText("Brand: ", brand) ||
        !readText("Model: ", model) ||
        !readText("License plate: ", plate) ||
        !readDouble("Base daily rate (INR): ", baseRate)) return false;

    switch (type) {
        case 1: {
            int seats = 0, automatic = 0;
            std::string fuel;
            if (!readInt("Seating capacity: ", seats) ||
                !readText("Fuel type (Petrol/Diesel/Hybrid/EV): ", fuel) ||
                !readInt("Automatic transmission? (1=yes, 0=no): ", automatic)) return false;
            if (automatic != 0 && automatic != 1) {
                std::cout << "Transmission choice must be 1 or 0. Vehicle not added.\n";
                return false;
            }
            app.fleet.addVehicle(std::make_unique<Car>(id, brand, model, plate,
                                                       baseRate, seats, fuel, automatic == 1));
            break;
        }
        case 2: {
            int cc = 0, geared = 0;
            if (!readInt("Engine capacity (cc): ", cc) ||
                !readInt("Has gears? (1=yes, 0=no): ", geared)) return false;
            if (geared != 0 && geared != 1) {
                std::cout << "Gear choice must be 1 or 0. Vehicle not added.\n";
                return false;
            }
            app.fleet.addVehicle(std::make_unique<Bike>(id, brand, model, plate,
                                                        baseRate, cc, geared == 1));
            break;
        }
        case 3: {
            double capacity = 0.0;
            int axles = 0;
            if (!readDouble("Cargo capacity (tons): ", capacity) ||
                !readInt("Number of axles: ", axles)) return false;
            app.fleet.addVehicle(std::make_unique<Truck>(id, brand, model, plate,
                                                         baseRate, capacity, axles));
            break;
        }
        default:
            std::cout << "Unknown vehicle type. Nothing was added.\n";
            return false;
    }
    std::cout << "Vehicle added successfully.\n";
    return true;
}

bool removeVehicle(AppContext& app) {
    int id = 0;
    if (!readInt("Vehicle ID to remove: ", id)) return false;
    app.fleet.removeVehicle(id);
    std::cout << "Vehicle removed successfully.\n";
    return true;
}

void listUsers(const AppContext& app) {
    auto items = app.fleet.listUsers();
    std::sort(items.begin(), items.end(), [](const User* lhs, const User* rhs) {
        return lhs->getUserId() < rhs->getUserId();
    });
    if (items.empty()) {
        std::cout << "No users are registered yet.\n";
        return;
    }
    for (const User* user : items) user->displayProfile(std::cout);
}

bool addUser(AppContext& app) {
    std::cout << "\nUSER TYPE\n  1. Customer\n  2. Employee\n  3. Admin\n";
    int type = 0, id = 0;
    std::string username, fullName, email;
    if (!readInt("Select type: ", type) ||
        !readInt("User ID: ", id) ||
        !readText("Username: ", username) ||
        !readText("Full name: ", fullName) ||
        !readText("Email: ", email)) return false;

    switch (type) {
        case 1: {
            std::string license, phone;
            if (!readText("Driving license: ", license) ||
                !readText("Phone number: ", phone)) return false;
            app.fleet.addUser(std::make_unique<Customer>(id, username, fullName,
                                                        email, license, phone));
            break;
        }
        case 2: {
            std::string employeeCode, department;
            if (!readText("Employee code: ", employeeCode) ||
                !readText("Department: ", department)) return false;
            app.fleet.addUser(std::make_unique<Employee>(id, username, fullName,
                                                        email, employeeCode, department));
            break;
        }
        case 3: {
            int clearance = 0;
            std::string office;
            if (!readInt("Security clearance level: ", clearance) ||
                !readText("Designated office: ", office)) return false;
            app.fleet.addUser(std::make_unique<Admin>(id, username, fullName,
                                                     email, clearance, office));
            break;
        }
        default:
            std::cout << "Unknown user type. Nothing was added.\n";
            return false;
    }
    std::cout << "User added successfully.\n";
    return true;
}

bool removeUser(AppContext& app) {
    int id = 0;
    if (!readInt("User ID to remove: ", id)) return false;
    app.fleet.removeUser(id);
    std::cout << "User removed successfully.\n";
    return true;
}

bool bookRental(AppContext& app) {
    int rentalId = 0, customerId = 0, vehicleId = 0;
    Date start, end;
    if (!readInt("New rental ID: ", rentalId) ||
        !readInt("Customer ID: ", customerId) ||
        !readInt("Vehicle ID: ", vehicleId) ||
        !readDate("Scheduled start date", start) ||
        !readDate("Scheduled end date", end)) return false;

    const DateRange range(start, end);
    const Vehicle* vehicle = app.fleet.getVehicle(vehicleId);
    if (!vehicle) {
        throw fleetflow::core::EntityNotFoundException(
            "Vehicle #" + std::to_string(vehicleId) + " not found.");
    }
    const double estimated = vehicle->calculateDailyRate() * range.durationDays();
    auto rental = std::make_unique<RentalRecord>(rentalId, customerId, vehicleId,
                                                 range, estimated);
    app.rentalService.bookRental(std::move(rental), currentLocalDate());

    std::cout << "Rental booked successfully. Estimated cost: ₹"
              << std::fixed << std::setprecision(2) << estimated << '\n';
    return true;
}

void listRentals(const AppContext& app) {
    auto items = app.rentalService.listRentals();
    std::sort(items.begin(), items.end(), [](const RentalRecord* lhs, const RentalRecord* rhs) {
        return lhs->getRentalId() < rhs->getRentalId();
    });
    if (items.empty()) {
        std::cout << "No rental bookings found.\n";
        return;
    }
    for (const RentalRecord* rental : items) rental->displayDetails(std::cout);
}

bool cancelRental(AppContext& app) {
    int id = 0;
    if (!readInt("Rental ID to cancel: ", id)) return false;
    app.rentalService.cancelRental(id);
    std::cout << "Rental cancelled successfully.\n";
    return true;
}

bool activateRental(AppContext& app) {
    int id = 0;
    Date start;
    if (!readInt("Rental ID to activate: ", id) ||
        !readDate("Actual pickup date", start)) return false;
    app.rentalService.activateRental(id, start);
    std::cout << "Rental activated successfully.\n";
    return true;
}

bool completeRental(AppContext& app) {
    int id = 0;
    Date returned;
    double finalCost = 0.0;
    if (!readInt("Rental ID to complete: ", id) ||
        !readDate("Actual return date", returned) ||
        !readDouble("Final settled cost (INR): ", finalCost)) return false;
    app.rentalService.completeRental(id, returned, finalCost);
    std::cout << "Rental completed successfully. Record any payment or refund separately.\n";
    return true;
}

bool recordPayment(AppContext& app) {
    int paymentId = 0, rentalId = 0, typeChoice = 0, methodChoice = 0;
    double amount = 0.0;
    Date date;
    std::string description;
    if (!readInt("Transaction ID: ", paymentId) ||
        !readInt("Rental ID: ", rentalId)) return false;

    RentalRecord* rental = app.rentalService.getRental(rentalId);
    if (!rental) {
        throw fleetflow::core::EntityNotFoundException(
            "Rental #" + std::to_string(rentalId) + " not found.");
    }
    const int customerId = rental->getCustomerId();

    std::cout << "Customer ID from rental: " << customerId << '\n'
              << "Transaction type: 1=Payment, 2=Refund\n";
    if (!readInt("Type: ", typeChoice) ||
        !readDouble("Amount (INR): ", amount) ||
        !readDate("Transaction date", date)) return false;

    std::cout << "Payment method: 1=Cash, 2=Card, 3=UPI\n";
    if (!readInt("Method: ", methodChoice) ||
        !readText("Description: ", description)) return false;

    fleetflow::core::PaymentType type;
    if (typeChoice == 1) type = fleetflow::core::PaymentType::Payment;
    else if (typeChoice == 2) type = fleetflow::core::PaymentType::Refund;
    else throw fleetflow::core::ValidationException("Transaction type must be 1 or 2.");

    fleetflow::core::PaymentMethod method;
    if (methodChoice == 1) method = fleetflow::core::PaymentMethod::Cash;
    else if (methodChoice == 2) method = fleetflow::core::PaymentMethod::Card;
    else if (methodChoice == 3) method = fleetflow::core::PaymentMethod::UPI;
    else throw fleetflow::core::ValidationException("Payment method must be 1, 2, or 3.");

    auto transaction = std::make_unique<PaymentRecord>(paymentId, rentalId, customerId,
                                                       amount, date, type, method, description);
    app.paymentService.recordTransaction(std::move(transaction));
    std::cout << "Transaction recorded successfully.\n";
    return true;
}

void listPayments(const AppContext& app) {
    auto items = app.paymentService.listTransactions();
    std::sort(items.begin(), items.end(), [](const PaymentRecord* lhs, const PaymentRecord* rhs) {
        return lhs->getPaymentId() < rhs->getPaymentId();
    });
    if (items.empty()) {
        std::cout << "No payment transactions found.\n";
        return;
    }
    for (const PaymentRecord* item : items) item->displayDetails(std::cout);
}

bool showRentalNetAmount(AppContext& app) {
    int rentalId = 0;
    if (!readInt("Rental ID: ", rentalId)) return false;
    const double net = app.paymentService.getNetAmountForRental(rentalId);
    std::cout << "Net recorded amount for rental #" << rentalId << ": ₹"
              << std::fixed << std::setprecision(2) << net << '\n';
    return false;
}

bool scheduleMaintenance(AppContext& app) {
    int id = 0, vehicleId = 0;
    double estimatedCost = 0.0;
    Date start;
    std::string description;
    if (!readInt("Maintenance ID: ", id) ||
        !readInt("Vehicle ID: ", vehicleId) ||
        !readText("Problem / service description: ", description) ||
        !readDate("Scheduled start date", start) ||
        !readDouble("Estimated cost (INR): ", estimatedCost)) return false;

    auto record = std::make_unique<MaintenanceRecord>(id, vehicleId, description,
                                                      start, estimatedCost);
    app.maintenanceService.scheduleMaintenance(std::move(record));
    std::cout << "Maintenance scheduled. The vehicle remains bookable until work starts.\n";
    return true;
}

void listMaintenance(const AppContext& app) {
    auto items = app.maintenanceService.listMaintenance();
    std::sort(items.begin(), items.end(), [](const MaintenanceRecord* lhs, const MaintenanceRecord* rhs) {
        return lhs->getMaintenanceId() < rhs->getMaintenanceId();
    });
    if (items.empty()) {
        std::cout << "No maintenance records found.\n";
        return;
    }
    for (const MaintenanceRecord* item : items) item->displayDetails(std::cout);
}

bool startMaintenance(AppContext& app) {
    int id = 0;
    if (!readInt("Maintenance ID to start: ", id)) return false;
    app.maintenanceService.startMaintenance(id);
    std::cout << "Maintenance started; vehicle marked UnderMaintenance.\n";
    return true;
}

bool completeMaintenance(AppContext& app) {
    int id = 0;
    Date finish;
    double actualCost = 0.0;
    if (!readInt("Maintenance ID to complete: ", id) ||
        !readDate("Completion date", finish) ||
        !readDouble("Actual cost (INR): ", actualCost)) return false;
    app.maintenanceService.completeMaintenance(id, finish, actualCost);
    std::cout << "Maintenance completed. Vehicle availability was updated by the service.\n";
    return true;
}

void printLoadReport(const fleetflow::storage::StorageReport& report) {
    std::cout << "\nLoaded existing data from CSV:\n"
              << "  Vehicles: " << report.loadedVehicles << '\n'
              << "  Users: " << report.loadedUsers << '\n'
              << "  Rentals: " << report.loadedRentals << '\n'
              << "  Payments: " << report.loadedPayments << '\n'
              << "  Maintenance: " << report.loadedMaintenance << '\n';

    if (!report.syntaxWarnings.empty()) {
        std::cout << "\nStorage warnings:\n";
        for (const auto& warning : report.syntaxWarnings) {
            std::cout << "  - " << warning << '\n';
        }
    }
    if (!report.quarantined.empty()) {
        std::cout << "\nQuarantined records (not loaded due to reference checks):\n";
        for (const auto& item : report.quarantined) {
            std::cout << "  - " << item.datasetName << " record #" << item.recordId
                      << ": " << item.reason << '\n';
        }
    }
}

} // namespace

int main() {
    try {
        AppContext app;
        printHeader();

        const auto report = app.storage.loadAll(app.vehicles, app.users, app.rentals,
                                                app.payments, app.maintenanceRecords);
        printLoadReport(report);

        bool running = true;
        while (running) {
            printMenu();
            int choice = -1;
            if (!readInt("\nSelect an option: ", choice)) break;

            try {
                bool changed = false;
                switch (choice) {
                    case 0:
                        saveAll(app);
                        running = false;
                        std::cout << "Goodbye. FleetFlow data has been saved.\n";
                        break;
                    case 1: showDashboard(app); break;
                    case 2: listVehicles(app); break;
                    case 3: changed = addVehicle(app); break;
                    case 4: changed = removeVehicle(app); break;
                    case 5: listUsers(app); break;
                    case 6: changed = addUser(app); break;
                    case 7: changed = removeUser(app); break;
                    case 8: changed = bookRental(app); break;
                    case 9: listRentals(app); break;
                    case 10: changed = cancelRental(app); break;
                    case 11: changed = activateRental(app); break;
                    case 12: changed = completeRental(app); break;
                    case 13: changed = recordPayment(app); break;
                    case 14: listPayments(app); break;
                    case 15: (void)showRentalNetAmount(app); break;
                    case 16: changed = scheduleMaintenance(app); break;
                    case 17: listMaintenance(app); break;
                    case 18: changed = startMaintenance(app); break;
                    case 19: changed = completeMaintenance(app); break;
                    case 20: saveAll(app); break;
                    default:
                        std::cout << "Unknown menu option. Choose a number from 0 to 20.\n";
                        break;
                }
                if (changed) saveAfterMutation(app);
            } catch (const std::exception& error) {
                std::cout << "\nOperation failed: " << error.what() << '\n';
            }
        }

        // EOF (for example, Ctrl+Z then Enter) also leaves the data saved.
        if (std::cin.eof()) {
            try {
                saveAll(app);
                std::cout << "Input closed. FleetFlow data has been saved.\n";
            } catch (const std::exception& error) {
                std::cerr << "Final save failed: " << error.what() << '\n';
                return 1;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FleetFlow could not start: " << error.what() << '\n';
        return 1;
    }
}
