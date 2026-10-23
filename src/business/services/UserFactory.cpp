#include "business/services/UserFactory.h"

#include "business/domain/Errors.h"

namespace lms::services {

using namespace lms::domain;

std::unique_ptr<User> UserFactory::create(Role role, int id, const std::string& username,
                                          const std::string& fullName, const std::string& number) {
    switch (role) {
        case Role::Administrator: return std::make_unique<Administrator>(id, username, fullName, number);
        case Role::Lecturer: return std::make_unique<Lecturer>(id, username, fullName, number);
        case Role::HeadOfDepartment: return std::make_unique<HeadOfDepartment>(id, username, fullName, number);
        case Role::Student: return std::make_unique<Student>(id, username, fullName, number);
    }
    throw RuleViolation("Unknown role");
}

}  // namespace lms::services
