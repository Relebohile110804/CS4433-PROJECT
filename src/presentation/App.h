#pragma once
#include <functional>

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
    App(Services services, ConsoleIO& io) : s_(services), io_(io) {}
    // Runs until the user chooses Exit or the input ends.
    void run();

private:
    void firstRun();
    void login();
    void changePassword(const domain::Session& s);
    void viewAuditLog(const domain::Session& s);
    // Runs one action and shows its error instead of stopping the program (NFR-09).
    void attempt(const std::function<void()>& action);

    // AdminMenu.cpp
    void adminMenu(const domain::Session& s);
    void adminUsers(const domain::Session& s);
    void adminCourses(const domain::Session& s);
    void adminLinks(const domain::Session& s);
    void adminBackup(const domain::Session& s);
    std::optional<int> pickUser(const domain::Session& s, std::optional<domain::Role> role, const std::string& title);
    std::optional<int> pickDepartment(const domain::Session& s);

    // LecturerMenu.cpp
    void lecturerMenu(const domain::Session& s);
    void createAssignment(const domain::Session& s);
    void grantExtension(const domain::Session& s);
    void markSubmissions(const domain::Session& s);

    // HodMenu.cpp
    void hodMenu(const domain::Session& s);
    void moderate(const domain::Session& s);
    void releaseMarks(const domain::Session& s);

    // StudentMenu.cpp
    void studentMenu(const domain::Session& s);
    void showMyAssignments(const domain::Session& s);
    void submitWork(const domain::Session& s);
    void showMyMark(const domain::Session& s);

    // Shared pickers (App.cpp): return nothing when the user cancels or the list is empty.
    std::optional<int> pickCourse(const domain::Session& s);
    std::optional<domain::Assignment> pickAssignment(const domain::Session& s, int courseId);
    std::optional<domain::Assignment> pickAssignmentFrom(const std::vector<domain::Assignment>& list);

    Services s_;
    ConsoleIO& io_;
};

}  // namespace lms::presentation
