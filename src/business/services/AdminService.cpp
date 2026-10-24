#include "business/services/AdminService.h"

#include "business/domain/Errors.h"
#include "business/services/AuthService.h"
#include "business/services/UserFactory.h"

namespace lms::services {

using namespace lms::domain;

// ---- first run ----

bool AdminService::needsFirstAdministrator() { return users_.list(Role::Administrator).empty(); }

int AdminService::createFirstAdministrator(const std::string& username, const std::string& fullName,
                                           const std::string& number, const std::string& password) {
    if (!needsFirstAdministrator()) throw RuleViolation("An administrator already exists");
    return addUser(Role::Administrator, username, fullName, number, password);
}

// ---- users ----

int AdminService::addUser(Role role, const std::string& username, const std::string& fullName,
                          const std::string& number, const std::string& password) {
    if (isBlank(username)) throw RuleViolation("Username cannot be empty");
    if (isBlank(number)) throw RuleViolation("Staff or student number cannot be empty");
    if (users_.findByUsername(username)) throw RuleViolation("The username " + username + " is already taken");
    if (users_.personNumberExists(number)) throw RuleViolation("The number " + number + " is already used");
    AuthService::checkPasswordRules(password);

    std::unique_ptr<User> user = UserFactory::create(role, 0, username, fullName, number);
    user->setPasswordHash(hasher_.hash(password));
    return users_.add(*user);
}

int AdminService::createUser(const Session& s, Role role, const std::string& username, const std::string& fullName,
                             const std::string& number, const std::string& password) {
    s.require(Permission::ManageUsers);
    return addUser(role, username, fullName, number, password);
}

void AdminService::updateUser(const Session& s, int userId, const std::string& fullName) {
    s.require(Permission::ManageUsers);
    std::unique_ptr<User> user = users_.findById(userId);
    if (!user) throw RuleViolation("User not found");
    user->setFullName(fullName);
    users_.update(*user);
}

void AdminService::resetPassword(const Session& s, int userId, const std::string& newPassword) {
    s.require(Permission::ManageUsers);
    std::unique_ptr<User> user = users_.findById(userId);
    if (!user) throw RuleViolation("User not found");
    AuthService::checkPasswordRules(newPassword);
    user->setPasswordHash(hasher_.hash(newPassword));
    user->clearFailedLogins();  // a reset also ends a lockout
    users_.update(*user);
}

void AdminService::deactivateUser(const Session& s, int userId) {
    s.require(Permission::ManageUsers);
    if (s.isSelf(userId)) throw RuleViolation("You cannot deactivate your own account");
    std::unique_ptr<User> user = users_.findById(userId);
    if (!user) throw RuleViolation("User not found");
    if (!user->isActive()) throw RuleViolation("This account is already deactivated");
    user->deactivate();
    users_.update(*user);
}

std::vector<std::unique_ptr<User>> AdminService::listUsers(const Session& s, std::optional<Role> role) {
    s.require(Permission::ManageUsers);
    return users_.list(role);
}

std::unique_ptr<User> AdminService::activeUserWithRole(int userId, Role role) {
    std::unique_ptr<User> user = users_.findById(userId);
    if (!user || user->getRole() != role) throw RuleViolation("No " + displayName(role) + " has that id");
    if (!user->isActive()) throw RuleViolation(user->getFullName() + " is deactivated");
    return user;
}

// ---- departments and courses ----

void AdminService::checkHodIsFree(int hodId, int departmentId) {
    activeUserWithRole(hodId, Role::HeadOfDepartment);
    std::optional<Department> current = courses_.departmentOfHod(hodId);
    if (current && current->getId() != departmentId)
        throw RuleViolation("This Head of Department already heads " + current->getName());
}

int AdminService::createDepartment(const Session& s, const std::string& name, int hodId) {
    s.require(Permission::ManageCourses);
    Department d(0, name, hodId);  // checks the name is not empty
    checkHodIsFree(hodId, 0);
    for (const Department& other : courses_.listDepartments())
        if (other.getName() == name) throw RuleViolation("A department called " + name + " already exists");
    return courses_.addDepartment(name, hodId);
}

void AdminService::updateDepartment(const Session& s, int departmentId, const std::string& name, int hodId) {
    s.require(Permission::ManageCourses);
    std::optional<Department> d = courses_.findDepartment(departmentId);
    if (!d) throw RuleViolation("Department not found");
    d->setName(name);
    checkHodIsFree(hodId, departmentId);
    for (const Department& other : courses_.listDepartments())
        if (other.getName() == name && other.getId() != departmentId)
            throw RuleViolation("A department called " + name + " already exists");
    d->setHodId(hodId);
    courses_.updateDepartment(*d);
}

std::vector<Department> AdminService::listDepartments(const Session& s) {
    s.require(Permission::ManageCourses);
    return courses_.listDepartments();
}

int AdminService::createCourse(const Session& s, int departmentId, const std::string& code, const std::string& title) {
    s.require(Permission::ManageCourses);
    Course c(0, departmentId, code, title);  // checks code and title are not empty
    if (!courses_.findDepartment(departmentId)) throw RuleViolation("Department not found");
    if (courses_.findCourseByCode(code)) throw RuleViolation("A course with code " + code + " already exists");
    return courses_.addCourse(departmentId, code, title);
}

void AdminService::updateCourse(const Session& s, int courseId, const std::string& code, const std::string& title) {
    s.require(Permission::ManageCourses);
    Course c = existingCourse(courseId);
    c.rename(code, title);
    std::optional<Course> same = courses_.findCourseByCode(code);
    if (same && same->getId() != courseId) throw RuleViolation("A course with code " + code + " already exists");
    courses_.updateCourse(c);
}

std::vector<Course> AdminService::listCourses(const Session& s) {
    s.require(Permission::ManageCourses);
    return courses_.listCourses();
}

Course AdminService::existingCourse(int courseId) {
    std::optional<Course> c = courses_.findCourse(courseId);
    if (!c) throw RuleViolation("Course not found");
    return *c;
}

// ---- lecturers and enrolment ----

void AdminService::assignLecturer(const Session& s, int courseId, int lecturerId) {
    s.require(Permission::ManageCourses);
    Course c = existingCourse(courseId);
    auto lecturer = activeUserWithRole(lecturerId, Role::Lecturer);
    if (courses_.isTeaching(lecturerId, courseId))
        throw RuleViolation(lecturer->getFullName() + " already teaches " + c.getCode());
    courses_.addLecturer(courseId, lecturerId);
}

void AdminService::removeLecturer(const Session& s, int courseId, int lecturerId) {
    s.require(Permission::ManageCourses);
    Course c = existingCourse(courseId);
    if (!courses_.isTeaching(lecturerId, courseId)) throw RuleViolation("That lecturer does not teach " + c.getCode());
    courses_.removeLecturer(courseId, lecturerId);
}

void AdminService::enrolStudent(const Session& s, int courseId, int studentId) {
    s.require(Permission::ManageEnrolment);
    Course c = existingCourse(courseId);
    auto student = activeUserWithRole(studentId, Role::Student);
    if (courses_.isEnrolled(studentId, courseId))
        throw RuleViolation(student->getFullName() + " is already enrolled in " + c.getCode());
    courses_.enrol(courseId, studentId);
}

void AdminService::withdrawStudent(const Session& s, int courseId, int studentId) {
    s.require(Permission::ManageEnrolment);
    Course c = existingCourse(courseId);
    if (!courses_.isEnrolled(studentId, courseId)) throw RuleViolation("That student is not enrolled in " + c.getCode());
    courses_.withdraw(courseId, studentId);
}

// ---- backup and restore ----

void AdminService::backup(const Session& s, const std::string& path) {
    s.require(Permission::BackupRestore);
    if (isBlank(path)) throw RuleViolation("Please give a file name for the backup");
    backup_.backupTo(path);
}

void AdminService::restore(const Session& s, const std::string& path) {
    s.require(Permission::BackupRestore);
    if (isBlank(path)) throw RuleViolation("Please give the backup file name");
    backup_.restoreFrom(path);
}

}  // namespace lms::services
