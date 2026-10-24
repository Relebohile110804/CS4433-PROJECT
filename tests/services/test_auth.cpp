#include "doctest/doctest.h"

#include "../TestHelpers.h"
#include "../fakes/FakeRepositories.h"
#include "../fakes/Fakes.h"
#include "business/domain/Errors.h"
#include "business/services/AuthService.h"

using namespace lms::domain;
using lms::services::AuthService;
using lms::services::UserFactory;

namespace {

// A user repository with one lecturer whose password is "correct-horse".
struct AuthFixture {
    fakes::FakeUserRepository users;
    fakes::FakePasswordHasher hasher;
    fakes::FakeClock clock{testutil::base()};
    AuthService auth{users, hasher, clock};
    int lecturerId;

    AuthFixture() {
        auto u = UserFactory::create(Role::Lecturer, 0, "tmokoena", "Thabo Mokoena", "S100");
        u->setPasswordHash(hasher.hash("correct-horse"));
        lecturerId = users.add(*u);
    }
    int failedLogins() { return users.findById(lecturerId)->getFailedLogins(); }
};

}  // namespace

TEST_CASE("a correct login returns a session for that user") {
    AuthFixture f;
    Session s = f.auth.login("tmokoena", "correct-horse");
    CHECK(s.getUser().getId() == f.lecturerId);
    CHECK(s.getUser().getRole() == Role::Lecturer);
}

TEST_CASE("a wrong password and an unknown username give the same message") {
    AuthFixture f;
    CHECK_THROWS_WITH_AS(f.auth.login("tmokoena", "wrong-pass"), "Wrong username or password", RuleViolation);
    CHECK_THROWS_WITH_AS(f.auth.login("nobody", "correct-horse"), "Wrong username or password", RuleViolation);
    CHECK(f.failedLogins() == 1);
}

TEST_CASE("FR-02: five failed logins lock the account for 15 minutes") {
    AuthFixture f;
    for (int i = 0; i < 4; ++i) CHECK_THROWS_WITH(f.auth.login("tmokoena", "wrong-pass"), "Wrong username or password");
    CHECK(f.failedLogins() == 4);
    CHECK_THROWS_WITH(f.auth.login("tmokoena", "wrong-pass"),
                      "Too many failed logins. This account is locked for 15 minutes");

    // Even the right password is refused while the account is locked.
    f.clock.advance(std::chrono::minutes(14));
    CHECK_THROWS_WITH(f.auth.login("tmokoena", "correct-horse"),
                      "This account is locked after too many failed logins. Try again in 15 minutes");

    // After 15 minutes the lock ends and a correct login clears it.
    f.clock.advance(std::chrono::minutes(1));
    CHECK_NOTHROW(f.auth.login("tmokoena", "correct-horse"));
    auto u = f.users.findById(f.lecturerId);
    CHECK(u->getFailedLogins() == 0);
    CHECK_FALSE(u->getLockedUntil().has_value());
}

TEST_CASE("a successful login resets the failed login count") {
    AuthFixture f;
    CHECK_THROWS(f.auth.login("tmokoena", "wrong-pass"));
    CHECK_THROWS(f.auth.login("tmokoena", "wrong-pass"));
    f.auth.login("tmokoena", "correct-horse");
    CHECK(f.failedLogins() == 0);
}

TEST_CASE("a deactivated account cannot log in") {
    AuthFixture f;
    auto u = f.users.findById(f.lecturerId);
    u->deactivate();
    f.users.update(*u);
    CHECK_THROWS_WITH(f.auth.login("tmokoena", "correct-horse"),
                      "This account has been deactivated. Please see the administrator");
}

TEST_CASE("FR-03: changing a password") {
    AuthFixture f;
    Session s = f.auth.login("tmokoena", "correct-horse");

    SUBCASE("the current password must be right") {
        CHECK_THROWS_WITH(f.auth.changePassword(s, "wrong-pass", "new-password1"), "The current password is wrong");
    }
    SUBCASE("the new password must have at least 8 characters") {
        CHECK_THROWS_WITH(f.auth.changePassword(s, "correct-horse", "short12"),
                          "The password must be at least 8 characters");
        CHECK_NOTHROW(f.auth.changePassword(s, "correct-horse", "exactly8"));
    }
    SUBCASE("the new password must be different") {
        CHECK_THROWS_WITH(f.auth.changePassword(s, "correct-horse", "correct-horse"),
                          "The new password must be different");
    }
    SUBCASE("after a change only the new password works") {
        f.auth.changePassword(s, "correct-horse", "new-password1");
        CHECK_NOTHROW(f.auth.login("tmokoena", "new-password1"));
        CHECK_THROWS(f.auth.login("tmokoena", "correct-horse"));
    }
}

TEST_CASE("nameOf shows the name and number for screens") {
    AuthFixture f;
    Session s = f.auth.login("tmokoena", "correct-horse");
    CHECK(f.auth.nameOf(s, f.lecturerId) == "Thabo Mokoena (S100)");
    CHECK(f.auth.nameOf(s, 999) == "Unknown user 999");
}

TEST_CASE("BR-05: a session refuses actions the role does not have") {
    AuthFixture f;
    Session s = f.auth.login("tmokoena", "correct-horse");
    CHECK_NOTHROW(s.require(Permission::MarkSubmission));
    CHECK_THROWS_AS(s.require(Permission::ReleaseMarks), AccessDenied);
    CHECK_THROWS_AS(s.require(Permission::ManageUsers), AccessDenied);
}
