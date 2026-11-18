#include "fleetflow/models/User.hpp"
#include "fleetflow/core/Exceptions.hpp"

namespace fleetflow::models {

namespace {

bool isBlank(const std::string& str) {
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

} // anonymous namespace

bool User::isValidEmail(const std::string& mail) {
    if (isBlank(mail)) {
        return false;
    }
    const auto atPos = mail.find('@');
    // Ensure single '@' that is neither first nor last character
    if (atPos == std::string::npos || atPos == 0 || atPos == mail.length() - 1) {
        return false;
    }
    if (mail.find('@', atPos + 1) != std::string::npos) {
        return false;
    }
    // Ensure '.' appears after '@' and is not immediately after '@' or at the end
    const auto dotPos = mail.find('.', atPos + 1);
    if (dotPos == std::string::npos || dotPos == atPos + 1 || dotPos == mail.length() - 1) {
        return false;
    }
    return true;
}

User::User(int id,
           std::string uname,
           std::string name,
           std::string mail,
           core::UserRole userRole)
    : role(userRole) {

    if (id <= 0) {
        throw core::ValidationException("User ID must be a positive integer, received: " + std::to_string(id));
    }
    if (isBlank(uname)) {
        throw core::ValidationException("Username cannot be empty or whitespace.");
    }
    if (isBlank(name)) {
        throw core::ValidationException("Full name cannot be empty or whitespace.");
    }
    if (!isValidEmail(mail)) {
        throw core::ValidationException("Invalid email format (expected user@domain.tld), received: " + mail);
    }

    userId = id;
    username = std::move(uname);
    fullName = std::move(name);
    email = std::move(mail);
}

void User::setEmail(const std::string& newEmail) {
    if (!isValidEmail(newEmail)) {
        throw core::ValidationException("Invalid email format (expected user@domain.tld), received: " + newEmail);
    }
    email = newEmail;
}

void User::setFullName(const std::string& newName) {
    if (isBlank(newName)) {
        throw core::ValidationException("Full name cannot be empty or whitespace.");
    }
    fullName = newName;
}

} // namespace fleetflow::models

