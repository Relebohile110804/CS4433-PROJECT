#include "business/domain/User.h"

#include "business/domain/Errors.h"

namespace lms::domain {

User::User(int id, std::string username, std::string fullName)
    : id_(id), username_(std::move(username)), fullName_(std::move(fullName)) {
    if (isBlank(username_)) throw RuleViolation("Username cannot be empty");
    if (isBlank(fullName_)) throw RuleViolation("Full name cannot be empty");
}

void User::setPasswordHash(const std::string& hash) {
    if (hash.empty()) throw RuleViolation("Password hash cannot be empty");
    passwordHash_ = hash;
}

void User::setFullName(const std::string& fullName) {
    if (isBlank(fullName)) throw RuleViolation("Full name cannot be empty");
    fullName_ = fullName;
}

void User::deactivate() { active_ = false; }

bool User::isLocked(TimePoint now) const { return lockedUntil_ && now < *lockedUntil_; }

void User::recordFailedLogin(TimePoint now) {
    ++failedLogins_;
    if (failedLogins_ >= MaxFailedLogins) {
        lockedUntil_ = now + LockoutTime;
        failedLogins_ = 0;  // the count starts again once the lockout ends
    }
}

void User::clearFailedLogins() {
    failedLogins_ = 0;
    lockedUntil_.reset();
}

void User::restoreLoginState(bool active, int failedLogins, std::optional<TimePoint> lockedUntil) {
    active_ = active;
    failedLogins_ = failedLogins;
    lockedUntil_ = lockedUntil;
}

Staff::Staff(int id, std::string username, std::string fullName, std::string staffNumber)
    : User(id, std::move(username), std::move(fullName)), staffNumber_(std::move(staffNumber)) {
    if (isBlank(staffNumber_)) throw RuleViolation("Staff number cannot be empty");
}

Student::Student(int id, std::string username, std::string fullName, std::string studentNumber)
    : User(id, std::move(username), std::move(fullName)), studentNumber_(std::move(studentNumber)) {
    if (isBlank(studentNumber_)) throw RuleViolation("Student number cannot be empty");
}

bool Administrator::can(Permission p) const {
    switch (p) {
        case Permission::ManageUsers:
        case Permission::ManageCourses:
        case Permission::ManageEnrolment:
        case Permission::BackupRestore:
        case Permission::ViewAuditLog:
            return true;
        default:
            return false;
    }
}

bool Lecturer::can(Permission p) const {
    switch (p) {
        case Permission::CreateAssignment:
        case Permission::GrantExtension:
        case Permission::MarkSubmission:
            return true;
        default:
            return false;
    }
}

bool HeadOfDepartment::can(Permission p) const {
    switch (p) {
        case Permission::ModerateMarks:
        case Permission::ReleaseMarks:
        case Permission::ViewAuditLog:
            return true;
        default:
            return false;
    }
}

bool Student::can(Permission p) const {
    return p == Permission::SubmitWork || p == Permission::ViewOwnMarks;
}

}  // namespace lms::domain
