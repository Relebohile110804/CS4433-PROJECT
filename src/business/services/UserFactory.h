#pragma once
#include <memory>
#include <string>

#include "business/domain/User.h"

namespace lms::services {

// Factory pattern (Section 2.2): the one place that turns a role into the right User subclass.
// Used by AdminService when an account is created and by SqliteUserRepository when a row is read.
class UserFactory {
public:
    static std::unique_ptr<domain::User> create(domain::Role role, int id, const std::string& username,
                                                const std::string& fullName, const std::string& number);
};

}  // namespace lms::services
