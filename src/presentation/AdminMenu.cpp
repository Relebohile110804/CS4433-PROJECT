#include "presentation/App.h"

namespace lms::presentation {

using namespace lms::domain;

void App::adminMenu(const Session& s) {
    for (;;) {
        int choice = io_.menu("Administrator", {"Users", "Departments and courses", "Lecturers and enrolment",
                                                "Backup and restore", "Audit log", "Change my password"},
                              "Log out");
        switch (choice) {
            case 0: return;
            case 1: adminUsers(s); break;
            case 2: adminCourses(s); break;
            case 3: adminLinks(s); break;
            case 4: adminBackup(s); break;
            case 5: attempt([&] { viewAuditLog(s); }); break;
            case 6: attempt([&] { changePassword(s); }); break;
            default: break;  // menu() only returns listed choices
        }
    }
}

std::optional<int> App::pickUser(const Session& s, std::optional<Role> role, const std::string& title) {
    auto users = s_.admin.listUsers(s, role);
    if (users.empty()) {
        io_.say("There are no users of that kind yet.");
        return std::nullopt;
    }
    std::vector<std::string> rows;
    rows.reserve(users.size());
    for (const auto& u : users) rows.push_back(userLine(*u));
    int choice = io_.menu(title, rows, "Cancel");
    if (choice == 0) return std::nullopt;
    return users[choice - 1]->getId();
}

std::optional<int> App::pickDepartment(const Session& s) {
    std::vector<Department> list = s_.admin.listDepartments(s);
    if (list.empty()) {
        io_.say("There are no departments yet.");
        return std::nullopt;
    }
    std::vector<std::string> rows;
    rows.reserve(list.size());
    for (const Department& d : list) rows.push_back(d.getName());
    int choice = io_.menu("Choose a department", rows, "Cancel");
    if (choice == 0) return std::nullopt;
    return list[choice - 1].getId();
}

void App::adminUsers(const Session& s) {
    int choice = io_.menu("Users", {"Create a user", "List users", "Edit a user's name", "Reset a password",
                                    "Deactivate a user"});
    attempt([&] {
        switch (choice) {
            case 1: createUser(s); break;
            case 2: listUsers(s); break;
            case 3: editUserName(s); break;
            case 4: resetPassword(s); break;
            case 5: deactivateUser(s); break;
            default: break;  // menu() only returns listed choices
        }
    });
}

std::string App::userLine(const User& u) {
    return u.getFullName() + "  " + u.getPersonNumber() + "  " + u.getUsername() + "  " + displayName(u.getRole()) +
           (u.isActive() ? "" : "  [deactivated]");
}

void App::createUser(const Session& s) {
    int r = io_.menu("Role", {"Administrator", "Lecturer", "Head of Department", "Student"}, "Cancel");
    if (r == 0) return;
    static const Role roles[] = {Role::Administrator, Role::Lecturer, Role::HeadOfDepartment, Role::Student};
    Role role = roles[r - 1];
    std::string username = io_.requiredText("Username");
    std::string fullName = io_.requiredText("Full name");
    std::string number = io_.requiredText(role == Role::Student ? "Student number" : "Staff number");
    std::string password = io_.requiredText("First password (at least 8 characters)");
    s_.admin.createUser(s, role, username, fullName, number, password);
    io_.say("User created.");
}

void App::listUsers(const Session& s) {
    auto users = s_.admin.listUsers(s, std::nullopt);
    io_.heading("All users (" + std::to_string(users.size()) + ")");
    for (const auto& u : users) io_.say(userLine(*u));
}

void App::editUserName(const Session& s) {
    auto id = pickUser(s, std::nullopt, "Choose a user");
    if (!id) return;
    s_.admin.updateUser(s, *id, io_.requiredText("New full name"));
    io_.say("Name changed.");
}

void App::resetPassword(const Session& s) {
    auto id = pickUser(s, std::nullopt, "Choose a user");
    if (!id) return;
    s_.admin.resetPassword(s, *id, io_.requiredText("New password (at least 8 characters)"));
    io_.say("Password reset. The account is unlocked.");
}

void App::deactivateUser(const Session& s) {
    auto id = pickUser(s, std::nullopt, "Choose a user");
    if (!id || !io_.confirm("Deactivate this account")) return;
    s_.admin.deactivateUser(s, *id);
    io_.say("Account deactivated.");
}

void App::adminCourses(const Session& s) {
    int choice = io_.menu("Departments and courses", {"Create a department", "Edit a department", "Create a course",
                                                      "Edit a course", "List departments and courses"});
    attempt([&] {
        switch (choice) {
            case 1: createDepartment(s); break;
            case 2: editDepartment(s); break;
            case 3: createCourse(s); break;
            case 4: editCourse(s); break;
            case 5: listDepartmentsAndCourses(s); break;
            default: break;  // menu() only returns listed choices
        }
    });
}

void App::createDepartment(const Session& s) {
    std::string name = io_.requiredText("Department name");
    auto hod = pickUser(s, Role::HeadOfDepartment, "Choose its Head of Department");
    if (!hod) return;
    s_.admin.createDepartment(s, name, *hod);
    io_.say("Department created.");
}

void App::editDepartment(const Session& s) {
    auto dept = pickDepartment(s);
    if (!dept) return;
    std::string name = io_.requiredText("Department name");
    auto hod = pickUser(s, Role::HeadOfDepartment, "Choose its Head of Department");
    if (!hod) return;
    s_.admin.updateDepartment(s, *dept, name, *hod);
    io_.say("Department changed.");
}

void App::createCourse(const Session& s) {
    auto dept = pickDepartment(s);
    if (!dept) return;
    std::string code = io_.requiredText("Course code");
    s_.admin.createCourse(s, *dept, code, io_.requiredText("Course title"));
    io_.say("Course created.");
}

void App::editCourse(const Session& s) {
    auto course = pickCourse(s);
    if (!course) return;
    std::string code = io_.requiredText("Course code");
    s_.admin.updateCourse(s, *course, code, io_.requiredText("Course title"));
    io_.say("Course changed.");
}

void App::listDepartmentsAndCourses(const Session& s) {
    auto courses = s_.admin.listCourses(s);
    for (const Department& d : s_.admin.listDepartments(s)) {
        io_.heading(d.getName() + ", HoD: " + s_.auth.nameOf(s, d.getHodId()));
        for (const Course& c : courses)
            if (c.getDepartmentId() == d.getId())
                io_.say("  " + c.getCode() + "  " + c.getTitle() + "  (" + std::to_string(c.getLecturerIds().size()) +
                        " lecturers, " + std::to_string(c.getStudentIds().size()) + " students)");
    }
}

void App::adminLinks(const Session& s) {
    int choice = io_.menu("Lecturers and enrolment", {"Assign a lecturer to a course", "Remove a lecturer from a course",
                                                      "Enrol a student", "Withdraw a student"});
    if (choice == 0) return;
    attempt([&] {
        auto course = pickCourse(s);
        if (!course) return;
        bool lecturer = choice <= 2;
        auto user = pickUser(s, lecturer ? Role::Lecturer : Role::Student,
                             lecturer ? "Choose a lecturer" : "Choose a student");
        if (!user) return;
        switch (choice) {
            case 1: s_.admin.assignLecturer(s, *course, *user); io_.say("Lecturer assigned."); break;
            case 2: s_.admin.removeLecturer(s, *course, *user); io_.say("Lecturer removed."); break;
            case 3: s_.admin.enrolStudent(s, *course, *user); io_.say("Student enrolled."); break;
            case 4: s_.admin.withdrawStudent(s, *course, *user); io_.say("Student withdrawn."); break;
            default: break;  // menu() only returns listed choices
        }
    });
}

void App::adminBackup(const Session& s) {
    int choice = io_.menu("Backup and restore", {"Back up the database", "Restore from a backup"});
    attempt([&] {
        if (choice == 1) {
            s_.admin.backup(s, io_.requiredText("Backup file name, for example backup-2026-11-01.db"));
            io_.say("Backup saved.");
        } else if (choice == 2) {
            std::string path = io_.requiredText("Backup file to restore");
            if (!io_.confirm("This replaces ALL current data with the backup. Continue")) return;
            s_.admin.restore(s, path);
            io_.say("Database restored. Please log out and log in again.");
        }
    });
}

}  // namespace lms::presentation
