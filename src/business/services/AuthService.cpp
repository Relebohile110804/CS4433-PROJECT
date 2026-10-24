#include "business/services/AuthService.h"

#include "business/domain/Errors.h"

namespace lms::services {

using namespace lms::domain;

namespace {
// The same message for an unknown username and a wrong password, so the
// screen does not reveal which usernames exist.
const char* const WrongLogin = "Wrong username or password";
}  // namespace

Session AuthService::login(const std::string& username, const std::string& password) {
    std::unique_ptr<User> user = users_.findByUsername(username);
    if (!user) throw RuleViolation(WrongLogin);

    TimePoint now = clock_.now();
    if (!user->isActive()) throw RuleViolation("This account has been deactivated. Please see the administrator");
    if (user->isLocked(now))
        throw RuleViolation("This account is locked after too many failed logins. Try again in 15 minutes");

    if (!hasher_.verify(user->getPasswordHash(), password)) {
        user->recordFailedLogin(now);  // FR-02: the fifth failure in a row locks the account
        users_.update(*user);
        if (user->isLocked(now))
            throw RuleViolation("Too many failed logins. This account is locked for 15 minutes");
        throw RuleViolation(WrongLogin);
    }

    if (user->getFailedLogins() != 0 || user->getLockedUntil()) {
        user->clearFailedLogins();
        users_.update(*user);
    }
    return Session(std::shared_ptr<const User>(std::move(user)), now);
}

void AuthService::checkPasswordRules(const std::string& password) {
    if (password.size() < MinPasswordLength)
        throw RuleViolation("The password must be at least " + std::to_string(MinPasswordLength) + " characters");
}

void AuthService::changePassword(const Session& s, const std::string& currentPassword,
                                 const std::string& newPassword) {
    // Read the account again so the check uses the stored hash, not the copy in the session.
    std::unique_ptr<User> user = users_.findById(s.getUser().getId());
    if (!user) throw RuleViolation("Your account no longer exists");
    if (!hasher_.verify(user->getPasswordHash(), currentPassword))
        throw RuleViolation("The current password is wrong");
    checkPasswordRules(newPassword);
    if (newPassword == currentPassword) throw RuleViolation("The new password must be different");
    user->setPasswordHash(hasher_.hash(newPassword));
    users_.update(*user);
}

std::string AuthService::nameOf(const Session&, int userId) {
    std::unique_ptr<User> user = users_.findById(userId);
    if (!user) return "Unknown user " + std::to_string(userId);
    return user->getFullName() + " (" + user->getPersonNumber() + ")";
}

}  // namespace lms::services
