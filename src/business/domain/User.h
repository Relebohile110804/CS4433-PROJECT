#pragma once
#include <optional>
#include <string>

#include "business/domain/Enums.h"
#include "business/domain/Time.h"

namespace lms::domain {

// Abstract base of every account. Each subclass answers getRole() and can(),
// so menus and services ask can() instead of testing the role (Figure 2).
class User {
public:
    static constexpr int MaxFailedLogins = 5;                         // FR-02
    static constexpr std::chrono::minutes LockoutTime{15};            // FR-02

    User(int id, std::string username, std::string fullName);
    virtual ~User() = default;

    virtual Role getRole() const = 0;
    virtual bool can(Permission p) const = 0;
    // Staff number or student number, stored in users.person_number.
    virtual const std::string& getPersonNumber() const = 0;

    void setPasswordHash(const std::string& hash);
    void deactivate();
    bool isLocked(TimePoint now) const;
    // Counts a failed login; the fifth one in a row locks the account for 15 minutes.
    void recordFailedLogin(TimePoint now);
    void clearFailedLogins();

    // Used by the repository to put back the login state read from the database.
    void restoreLoginState(bool active, int failedLogins, std::optional<TimePoint> lockedUntil);

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    const std::string& getUsername() const { return username_; }
    const std::string& getPasswordHash() const { return passwordHash_; }
    const std::string& getFullName() const { return fullName_; }
    void setFullName(const std::string& fullName);
    bool isActive() const { return active_; }
    int getFailedLogins() const { return failedLogins_; }
    const std::optional<TimePoint>& getLockedUntil() const { return lockedUntil_; }

private:
    int id_;
    std::string username_;
    std::string passwordHash_;
    std::string fullName_;
    bool active_ = true;
    int failedLogins_ = 0;
    std::optional<TimePoint> lockedUntil_;
};

class Staff : public User {
public:
    Staff(int id, std::string username, std::string fullName, std::string staffNumber);
    const std::string& getPersonNumber() const override { return staffNumber_; }

private:
    std::string staffNumber_;
};

class Administrator : public Staff {
public:
    using Staff::Staff;
    Role getRole() const override { return Role::Administrator; }
    bool can(Permission p) const override;
};

class Lecturer : public Staff {
public:
    using Staff::Staff;
    Role getRole() const override { return Role::Lecturer; }
    bool can(Permission p) const override;
};

class HeadOfDepartment : public Staff {
public:
    using Staff::Staff;
    Role getRole() const override { return Role::HeadOfDepartment; }
    bool can(Permission p) const override;
};

class Student : public User {
public:
    Student(int id, std::string username, std::string fullName, std::string studentNumber);
    Role getRole() const override { return Role::Student; }
    bool can(Permission p) const override;
    const std::string& getPersonNumber() const override { return studentNumber_; }

private:
    std::string studentNumber_;
};

}  // namespace lms::domain
