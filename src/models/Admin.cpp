#include "fleetflow/models/Admin.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include <ostream>

namespace fleetflow::models {

namespace {

bool isBlank(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

} // anonymous namespace

Admin::Admin(int id,
             std::string uname,
             std::string name,
             std::string mail,
             int clearanceLevel,
             std::string office)
    : User(id, std::move(uname), std::move(name), std::move(mail), core::UserRole::Admin) {

    if (clearanceLevel < 1) {
        throw core::ValidationException("Admin security clearance level must be at least 1, received: " + std::to_string(clearanceLevel));
    }
    if (isBlank(office)) {
        throw core::ValidationException("Admin designated office cannot be empty or whitespace.");
    }

    securityClearance = clearanceLevel;
    designatedOffice = std::move(office);
}

void Admin::setDesignatedOffice(const std::string& newOffice) {
    if (isBlank(newOffice)) {
        throw core::ValidationException("Admin designated office cannot be empty or whitespace.");
    }
    designatedOffice = newOffice;
}

std::string Admin::getRoleName() const {
    return "Admin";
}

void Admin::displayProfile(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [USER PROFILE: ADMINISTRATOR]\n"
       << " User ID         : " << getUserId() << "\n"
       << " Username        : " << getUsername() << "\n"
       << " Full Name       : " << getFullName() << "\n"
       << " Email           : " << getEmail() << "\n"
       << " Clearance Level : Tier " << securityClearance << "\n"
       << " Designated Office: " << designatedOffice << "\n"
       << " System Role     : " << core::toString(getRole()) << "\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

