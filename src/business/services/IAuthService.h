#pragma once
#include <string>

#include "business/domain/Session.h"

namespace lms::services {

class IAuthService {
public:
    virtual ~IAuthService() = default;
    // UC-01: returns a session for the user, or throws RuleViolation with a message to show.
    virtual domain::Session login(const std::string& username, const std::string& password) = 0;
    // UC-02 / FR-03: the current password must be right and the new one at least 8 characters.
    virtual void changePassword(const domain::Session& s, const std::string& currentPassword,
                                const std::string& newPassword) = 0;
};

}  // namespace lms::services
