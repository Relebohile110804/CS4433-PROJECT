#include "data/SqliteUserRepository.h"

#include "business/services/UserFactory.h"
#include "data/TimeText.h"

namespace lms::data {

using namespace lms::domain;

namespace {

const char* const SelectUser =
    "SELECT user_id, username, password_hash, full_name, role, person_number, active, failed_logins, locked_until "
    "FROM users ";

// Builds the right subclass through UserFactory, then puts back the stored login state.
std::unique_ptr<User> readUser(const Statement& st) {
    auto user = services::UserFactory::create(roleFromCode(st.columnText(4)), st.columnInt(0), st.columnText(1),
                                              st.columnText(3), st.columnText(5));
    user->setPasswordHash(st.columnText(2));
    std::optional<TimePoint> lockedUntil;
    if (!st.isNull(8)) lockedUntil = fromIsoText(st.columnText(8));
    user->restoreLoginState(st.columnInt(6) == 1, st.columnInt(7), lockedUntil);
    return user;
}

void bindLockedUntil(Statement& st, int index, const User& user) {
    if (const auto& lockedUntil = user.getLockedUntil())
        st.bindText(index, toIsoText(*lockedUntil));
    else
        st.bindNull(index);
}

}  // namespace

std::unique_ptr<User> SqliteUserRepository::findById(int id) {
    Statement st = db_.prepare(std::string(SelectUser) + "WHERE user_id = ?1");
    st.bindInt(1, id);
    return st.step() ? readUser(st) : nullptr;
}

std::unique_ptr<User> SqliteUserRepository::findByUsername(const std::string& username) {
    Statement st = db_.prepare(std::string(SelectUser) + "WHERE username = ?1");
    st.bindText(1, username);
    return st.step() ? readUser(st) : nullptr;
}

bool SqliteUserRepository::personNumberExists(const std::string& number) {
    Statement st = db_.prepare("SELECT 1 FROM users WHERE person_number = ?1");
    st.bindText(1, number);
    return st.step();
}

std::vector<std::unique_ptr<User>> SqliteUserRepository::list(std::optional<Role> role) {
    Statement st = db_.prepare(std::string(SelectUser) + "WHERE ?1 IS NULL OR role = ?1 ORDER BY full_name, user_id");
    if (role)
        st.bindText(1, toCode(*role));
    else
        st.bindNull(1);
    std::vector<std::unique_ptr<User>> users;
    while (st.step()) users.push_back(readUser(st));
    return users;
}

int SqliteUserRepository::add(const User& user) {
    Statement st = db_.prepare(
        "INSERT INTO users (username, password_hash, full_name, role, person_number, active, failed_logins, "
        "locked_until) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
    st.bindText(1, user.getUsername())
        .bindText(2, user.getPasswordHash())
        .bindText(3, user.getFullName())
        .bindText(4, toCode(user.getRole()))
        .bindText(5, user.getPersonNumber())
        .bindInt(6, user.isActive() ? 1 : 0)
        .bindInt(7, user.getFailedLogins());
    bindLockedUntil(st, 8, user);
    st.run();
    return static_cast<int>(db_.lastInsertId());
}

void SqliteUserRepository::update(const User& user) {
    // Username, role and number never change after creation (FR-04), so they are not updated.
    Statement st = db_.prepare(
        "UPDATE users SET password_hash = ?2, full_name = ?3, active = ?4, failed_logins = ?5, locked_until = ?6 "
        "WHERE user_id = ?1");
    st.bindInt(1, user.getId())
        .bindText(2, user.getPasswordHash())
        .bindText(3, user.getFullName())
        .bindInt(4, user.isActive() ? 1 : 0)
        .bindInt(5, user.getFailedLogins());
    bindLockedUntil(st, 6, user);
    st.run();
}

}  // namespace lms::data
