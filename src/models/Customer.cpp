#include "fleetflow/models/Customer.hpp"
#include "fleetflow/core/Exceptions.hpp"
#include <ostream>

namespace fleetflow::models {

namespace {

bool isBlank(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

} // anonymous namespace

Customer::Customer(int id,
                   std::string uname,
                   std::string name,
                   std::string mail,
                   std::string license,
                   std::string phone)
    : User(id, std::move(uname), std::move(name), std::move(mail), core::UserRole::Customer) {

    if (isBlank(license)) {
        throw core::ValidationException("Customer driving license cannot be empty or whitespace.");
    }
    if (isBlank(phone)) {
        throw core::ValidationException("Customer phone number cannot be empty or whitespace.");
    }

    drivingLicense = std::move(license);
    phoneNumber = std::move(phone);
}

void Customer::setPhoneNumber(const std::string& newPhone) {
    if (isBlank(newPhone)) {
        throw core::ValidationException("Customer phone number cannot be empty or whitespace.");
    }
    phoneNumber = newPhone;
}

std::string Customer::getRoleName() const {
    return "Customer";
}

void Customer::displayProfile(std::ostream& os) const {
    os << "----------------------------------------\n"
       << " [USER PROFILE: CUSTOMER]\n"
       << " User ID         : " << getUserId() << "\n"
       << " Username        : " << getUsername() << "\n"
       << " Full Name       : " << getFullName() << "\n"
       << " Email           : " << getEmail() << "\n"
       << " Driving License : " << drivingLicense << "\n"
       << " Phone Number    : " << phoneNumber << "\n"
       << " System Role     : " << core::toString(getRole()) << "\n"
       << "----------------------------------------\n";
}

} // namespace fleetflow::models

