#pragma once
#include <memory>

#include "business/domain/Time.h"
#include "business/domain/User.h"

namespace lms::domain {

// The logged-in caller. Every service operation receives it so it can check the role (NFR-05).
class Session {
public:
    Session(std::shared_ptr<const User> user, TimePoint startedAt);

    const User& getUser() const { return *user_; }
    TimePoint getStartedAt() const { return startedAt_; }
    // Throws AccessDenied if the user's role does not have the permission.
    void require(Permission p) const;
    bool isSelf(int userId) const { return user_->getId() == userId; }

private:
    std::shared_ptr<const User> user_;
    TimePoint startedAt_;
};

}  // namespace lms::domain
