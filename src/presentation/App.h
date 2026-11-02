#pragma once
#include <functional>

#include "business/ports/IErrorLog.h"
#include "business/services/IAdminService.h"
#include "business/services/IAssignmentService.h"
#include "business/services/IAuditService.h"
#include "business/services/IAuthService.h"
#include "business/services/IMarkingService.h"
#include "business/services/IModerationService.h"
#include "business/services/ISubmissionService.h"
#include "presentation/ConsoleIO.h"

namespace lms::presentation {

// The services the menus use. main() creates them and passes them in.
struct Services {
    services::IAuthService& auth;
    services::IAdminService& admin;
    services::IAssignmentService& assignments;
    services::ISubmissionService& submissions;
    services::IMarkingService& marking;
    services::IModerationService& moderation;
    services::IAuditService& audit;
};

// Role Menus (Figure 1): the login screen and one menu for each role. Every function
// is at most 3 choices away from the role's main menu (NFR-10).
class App {
public:
    // Unexpected errors (database or file) are also written to the error log, if one is given.
    App(Services services, ConsoleIO& io, ports::IErrorLog* errorLog = nullptr)
        : s_(services), io_(io), log_(errorLog) {}
    // Runs until the user chooses Exit or the input ends.
    void run();

private:
    void firstRun();
    void login();
    void changePassword(const domain::Session& s);
    void viewAuditLog(const domain::Session& s);
    // Runs one action and shows its error instead of stopping the program (NFR-09).
    void attempt(const std::function<void()>& action);
    // Writes an unexpected error to the error log (UC-09 E4, UC-16 E4, UC-20 E2).
    void logError(const std::exception& e);
    // The course code of an assignment's course, from the user's own courses.
    std::string courseCode(const domain::Session& s, int courseId);

    // AdminMenu.cpp
    void adminMenu(const domain::Session& s);
    void adminUsers(const domain::Session& s);
    void adminCourses(const domain::Session& s);
    void adminLinks(const domain::Session& s);
    void adminBackup(const domain::Session& s);
    std::optional<int> pickUser(const domain::Session& s, std::optional<domain::Role> role, const std::string& title);
    std::optional<int> pickDepartment(const domain::Session& s);
    static std::string userLine(const domain::User& u);
    void createUser(const domain::Session& s);
    void listUsers(const domain::Session& s);
    void editUserName(const domain::Session& s);
    void resetPassword(const domain::Session& s);
    void deactivateUser(const domain::Session& s);
    void createDepartment(const domain::Session& s);
    void editDepartment(const domain::Session& s);
    void createCourse(const domain::Session& s);
    void editCourse(const domain::Session& s);
    void listDepartmentsAndCourses(const domain::Session& s);

    // LecturerMenu.cpp
    void lecturerMenu(const domain::Session& s);
    void createAssignment(const domain::Session& s);
    void grantExtension(const domain::Session& s);
    void markSubmissions(const domain::Session& s);
    ConsoleIO::Option markingRow(const domain::Session& s, const domain::Submission& sub);
    void markOne(const domain::Session& s, const domain::Assignment& a, const domain::Submission& sub);

    // HodMenu.cpp
    void hodMenu(const domain::Session& s);
    void moderate(const domain::Session& s);
    void releaseMarks(const domain::Session& s);
    ConsoleIO::Option queueRow(const domain::Session& s, const domain::Assignment& a);
    ConsoleIO::Option moderationRow(const domain::Session& s, const domain::Assignment& a,
                                    const domain::Submission& sub);
    void moderateOne(const domain::Session& s, const domain::Assignment& a, const domain::Submission& sub);

    // StudentMenu.cpp
    void studentMenu(const domain::Session& s);
    void showMyAssignments(const domain::Session& s);
    void submitWork(const domain::Session& s);
    // The steps of UC-09: each returns nothing (or false) when the student cannot go on.
    bool canSubmit(const domain::Session& s, const domain::Assignment& a);
    std::optional<std::string> askForFile();
    void showLateWarning(int daysLate);
    std::optional<domain::SubmissionVersion> storeSubmission(const domain::Session& s, const domain::Assignment& a,
                                                             const std::string& path);
    void showMyMark(const domain::Session& s);
    // What a student is shown: a mark that is not released is just "Marked, awaiting release" (A8).
    static std::string studentStatus(domain::SubmissionStatus status);

    // Shared pickers (App.cpp): return nothing when the user cancels or the list is empty.
    std::optional<int> pickCourse(const domain::Session& s);
    std::optional<domain::Assignment> pickAssignment(const domain::Session& s, int courseId);
    std::optional<domain::Assignment> pickAssignmentFrom(const std::vector<domain::Assignment>& list);

    Services s_;
    ConsoleIO& io_;
    ports::IErrorLog* log_;
};

}  // namespace lms::presentation
