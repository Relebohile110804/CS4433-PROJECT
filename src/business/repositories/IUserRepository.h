#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "business/domain/User.h"

namespace lms::repositories {

class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    virtual std::unique_ptr<domain::User> findById(int id) = 0;
    virtual std::unique_ptr<domain::User> findByUsername(const std::string& username) = 0;
    virtual bool personNumberExists(const std::string& number) = 0;
    // All users, or only those with the given role, ordered by name.
    virtual std::vector<std::unique_ptr<domain::User>> list(std::optional<domain::Role> role) = 0;
    // Inserts the user and returns the new id.
    virtual int add(const domain::User& user) = 0;
    virtual void update(const domain::User& user) = 0;
};

}  // namespace lms::repositories
