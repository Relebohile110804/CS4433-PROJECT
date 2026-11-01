#include "catch2/catch_amalgamated.hpp"

#include "../TestHelpers.h"
#include "business/domain/Errors.h"
#include "business/domain/Session.h"
#include "business/services/UserFactory.h"

using namespace lms::domain;
using lms::services::UserFactory;
using testutil::base;

TEST_CASE("UserFactory creates the right subclass for each role", "[unit][domain]") {
    for (Role r : {Role::Administrator, Role::Lecturer, Role::HeadOfDepartment, Role::Student}) {
        auto u = UserFactory::create(r, 1, "user", "Some Name", "N001");
        CHECK(u->getRole() == r);
        CHECK(u->getPersonNumber() == "N001");
        CHECK(u->isActive());
    }
}

TEST_CASE("each role has only its own permissions", "[unit][domain]") {
    auto admin = UserFactory::create(Role::Administrator, 1, "a", "A", "S1");
    auto lect = UserFactory::create(Role::Lecturer, 2, "l", "L", "S2");
    auto hod = UserFactory::create(Role::HeadOfDepartment, 3, "h", "H", "S3");
    auto stud = UserFactory::create(Role::Student, 4, "s", "S", "2023001");

    CHECK(admin->can(Permission::ManageUsers));
    CHECK(admin->can(Permission::BackupRestore));
    CHECK_FALSE(admin->can(Permission::MarkSubmission));
    CHECK_FALSE(admin->can(Permission::ReleaseMarks));

    CHECK(lect->can(Permission::CreateAssignment));
    CHECK(lect->can(Permission::GrantExtension));
    CHECK(lect->can(Permission::MarkSubmission));
    CHECK_FALSE(lect->can(Permission::ModerateMarks));
    CHECK_FALSE(lect->can(Permission::ViewAuditLog));

    CHECK(hod->can(Permission::ModerateMarks));
    CHECK(hod->can(Permission::ReleaseMarks));
    CHECK(hod->can(Permission::ViewAuditLog));
    CHECK_FALSE(hod->can(Permission::MarkSubmission));

    CHECK(stud->can(Permission::SubmitWork));
    CHECK(stud->can(Permission::ViewOwnMarks));
    CHECK_FALSE(stud->can(Permission::CreateAssignment));
    CHECK_FALSE(stud->can(Permission::ManageUsers));
}

TEST_CASE("blank names and numbers are refused", "[unit][domain]") {
    CHECK_THROWS_AS(Student(1, " ", "Name", "2023001"), RuleViolation);
    CHECK_THROWS_AS(Student(1, "user", "", "2023001"), RuleViolation);
    CHECK_THROWS_AS(Lecturer(1, "user", "Name", "  "), RuleViolation);
}

TEST_CASE("five failed logins lock the account for 15 minutes (FR-02)", "[unit][domain]") {
    Student s(1, "s", "S", "2023001");
    for (int i = 0; i < 4; ++i) s.recordFailedLogin(base());
    CHECK_FALSE(s.isLocked(base()));
    CHECK(s.getFailedLogins() == 4);

    s.recordFailedLogin(base());
    CHECK(s.isLocked(base()));
    CHECK(s.isLocked(base() + std::chrono::minutes(14) + std::chrono::seconds(59)));
    CHECK_FALSE(s.isLocked(base() + std::chrono::minutes(15)));
}

TEST_CASE("a successful login clears the failed count", "[unit][domain]") {
    Student s(1, "s", "S", "2023001");
    s.recordFailedLogin(base());
    s.recordFailedLogin(base());
    s.clearFailedLogins();
    CHECK(s.getFailedLogins() == 0);
    CHECK_FALSE(s.getLockedUntil().has_value());
}

TEST_CASE("deactivate and password hash", "[unit][domain]") {
    Lecturer l(1, "l", "L", "S1");
    l.setPasswordHash("$argon2id$...");
    CHECK(l.getPasswordHash() == "$argon2id$...");
    CHECK_THROWS_AS(l.setPasswordHash(""), RuleViolation);
    l.deactivate();
    CHECK_FALSE(l.isActive());
}

TEST_CASE("Session.require refuses a permission the role does not have (NFR-05)", "[unit][domain]") {
    auto stud = std::shared_ptr<const User>(UserFactory::create(Role::Student, 7, "s", "S", "2023001"));
    Session session(stud, base());
    CHECK_NOTHROW(session.require(Permission::SubmitWork));
    CHECK_THROWS_AS(session.require(Permission::MarkSubmission), AccessDenied);
    CHECK(session.isSelf(7));
    CHECK_FALSE(session.isSelf(8));
}

TEST_CASE("role and status codes match the database CHECK constraints", "[unit][domain]") {
    for (Role r : {Role::Administrator, Role::Lecturer, Role::HeadOfDepartment, Role::Student})
        CHECK(roleFromCode(toCode(r)) == r);
    CHECK(toCode(Role::HeadOfDepartment) == "HOD");
    for (SubmissionStatus s : {SubmissionStatus::Submitted, SubmissionStatus::Late, SubmissionStatus::Marked,
                               SubmissionStatus::Returned, SubmissionStatus::Moderated, SubmissionStatus::Released})
        CHECK(statusFromCode(toCode(s)) == s);
    CHECK_THROWS_AS(roleFromCode("TEACHER"), RuleViolation);
}
