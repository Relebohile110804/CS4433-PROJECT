#include "business/domain/Session.h"

#include "business/domain/Errors.h"

namespace lms::domain {

Session::Session(std::shared_ptr<const User> user, TimePoint startedAt)
    : user_(std::move(user)), startedAt_(startedAt) {
    if (!user_) throw RuleViolation("A session needs a user");
}

void Session::require(Permission p) const {
    if (!user_->can(p))
        throw AccessDenied("A " + displayName(user_->getRole()) + " is not allowed to do this");
}

}  // namespace lms::domain
