#include "presentation/App.h"

#include "business/domain/Errors.h"

namespace lms::presentation {

using namespace lms::domain;

void App::attempt(const std::function<void()>& action) {
    try {
        action();
    } catch (const InputClosed&) {
        throw;
    } catch (const AccessDenied& e) {
        io_.error(std::string("Not allowed: ") + e.what());
    } catch (const std::exception& e) {
        io_.error(e.what());
    }
}

void App::run() {
    try {
        if (s_.admin.needsFirstAdministrator()) firstRun();
        for (;;) {
            int choice = io_.menu("Moderated LMS", {"Log in"}, "Exit");
            if (choice == 0) break;
            attempt([&] { login(); });
        }
    } catch (const InputClosed&) {
    }
    io_.say("Goodbye.");
}

void App::firstRun() {
    io_.heading("First run");
    io_.say("There is no administrator yet. Please create the first account.");
    for (;;) {
        std::string username = io_.requiredText("Username");
        std::string fullName = io_.requiredText("Full name");
        std::string number = io_.requiredText("Staff number");
        std::string password = io_.requiredText("Password (at least 8 characters)");
        try {
            s_.admin.createFirstAdministrator(username, fullName, number, password);
            io_.say("Administrator created. You can now log in.");
            return;
        } catch (const RuleViolation& e) {
            io_.error(e.what());
        }
    }
}

void App::login() {
    io_.heading("Log in");
    std::string username = io_.requiredText("Username");
    std::string password = io_.requiredText("Password");
    Session session = s_.auth.login(username, password);
    const User& user = session.getUser();
    io_.say("Welcome, " + user.getFullName() + " (" + displayName(user.getRole()) + ").");
    switch (user.getRole()) {
        case Role::Administrator: adminMenu(session); break;
        case Role::Lecturer: lecturerMenu(session); break;
        case Role::HeadOfDepartment: hodMenu(session); break;
        case Role::Student: studentMenu(session); break;
    }
    io_.say("Logged out.");
}

void App::changePassword(const Session& s) {
    io_.heading("Change password");
    std::string current = io_.requiredText("Current password");
    std::string fresh = io_.requiredText("New password (at least 8 characters)");
    std::string again = io_.requiredText("New password again");
    if (fresh != again) {
        io_.error("The two new passwords are different. Nothing was changed.");
        return;
    }
    s_.auth.changePassword(s, current, fresh);
    io_.say("Password changed.");
}

void App::viewAuditLog(const Session& s) {
    std::optional<int> courseId;
    std::optional<int> assignmentId;
    int scope = io_.menu("Audit log", {"All entries", "One course", "One assignment"});
    if (scope == 0) return;
    if (scope >= 2) {
        courseId = pickCourse(s);
        if (!courseId) return;
    }
    if (scope == 3) {
        std::optional<Assignment> a = pickAssignment(s, *courseId);
        if (!a) return;
        assignmentId = a->getId();
    }
    std::vector<AuditEntry> entries = s_.audit.viewLog(s, courseId, assignmentId);
    io_.heading("Audit log (" + std::to_string(entries.size()) + " entries)");
    for (const AuditEntry& e : entries) {
        std::string oldValue = e.getOldValue() ? ConsoleIO::formatMark(*e.getOldValue()) : "-";
        io_.say(ConsoleIO::fit(ConsoleIO::formatTime(e.getChangedAt()) + "  submission " +
                               std::to_string(e.getSubmissionId()) + "  " + oldValue + " -> " +
                               ConsoleIO::formatMark(e.getNewValue()) + "  by " +
                               s_.auth.nameOf(s, e.getChangedBy())));
        io_.say(ConsoleIO::fit("    Reason: " + e.getReason()));
    }
}

std::optional<int> App::pickCourse(const Session& s) {
    std::vector<Course> courses = s_.assignments.myCourses(s);
    if (courses.empty()) {
        io_.say("You have no courses yet.");
        return std::nullopt;
    }
    std::vector<std::string> rows;
    for (const Course& c : courses) rows.push_back(c.getCode() + "  " + c.getTitle());
    int choice = io_.menu("Choose a course", rows, "Cancel");
    if (choice == 0) return std::nullopt;
    return courses[choice - 1].getId();
}

std::optional<Assignment> App::pickAssignment(const Session& s, int courseId) {
    return pickAssignmentFrom(s_.assignments.listForCourse(s, courseId));
}

std::optional<Assignment> App::pickAssignmentFrom(const std::vector<Assignment>& list) {
    if (list.empty()) {
        io_.say("There are no assignments here.");
        return std::nullopt;
    }
    std::vector<std::string> rows;
    for (const Assignment& a : list)
        rows.push_back(a.getTitle() + "  (due " + ConsoleIO::formatTime(a.getDeadline()) + ", out of " +
                       std::to_string(a.getMaxMark()) + ")");
    int choice = io_.menu("Choose an assignment", rows, "Cancel");
    if (choice == 0) return std::nullopt;
    return list[choice - 1];
}

}  // namespace lms::presentation
