// Scripted console sessions: the menus are driven by typed input, as a user would.
#include "doctest/doctest.h"

#include <sstream>

#include "../TestHelpers.h"
#include "../fakes/Fakes.h"
#include "business/services/AdminService.h"
#include "business/services/AssignmentService.h"
#include "business/services/AuditService.h"
#include "business/services/AuthService.h"
#include "business/services/MarkingService.h"
#include "business/services/ModerationService.h"
#include "business/services/SubmissionService.h"
#include "business/services/TenPercentPerDayPolicy.h"
#include "data/Database.h"
#include "data/SqliteAssignmentRepository.h"
#include "data/SqliteAuditRepository.h"
#include "data/SqliteCourseRepository.h"
#include "data/SqliteSubmissionRepository.h"
#include "data/SqliteUserRepository.h"
#include "presentation/App.h"

using namespace lms;

namespace {

// The same wiring as main.cpp, but with an in-memory database and fakes for the outside world.
struct Program {
    data::Database db{":memory:"};
    data::SqliteUserRepository users{db};
    data::SqliteCourseRepository courses{db};
    data::SqliteAssignmentRepository assignmentRepo{db};
    data::SqliteSubmissionRepository submissionRepo{db};
    data::SqliteAuditRepository auditRepo{db};
    fakes::FakePasswordHasher hasher;
    fakes::FakeFileStore files;
    fakes::FakeClock clock{testutil::base()};
    services::TenPercentPerDayPolicy policy;
    services::AuthService auth{users, hasher, clock};
    services::AdminService admin{users, courses, hasher, db};
    services::AssignmentService assignments{assignmentRepo, courses, clock};
    services::AuditService audit{auditRepo, courses, clock};
    services::SubmissionService submitting{submissionRepo, assignmentRepo, courses, files, policy, clock, db};
    services::MarkingService marking{submissionRepo, assignmentRepo, courses, policy, audit, clock, db};
    services::ModerationService moderation{submissionRepo, assignmentRepo, courses, audit, clock, db};

    std::string run(const std::string& input) {
        std::istringstream in(input);
        std::ostringstream out;
        presentation::ConsoleIO io(in, out);
        presentation::App app({auth, admin, assignments, submitting, marking, moderation, audit}, io);
        app.run();
        return out.str();
    }
};

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("first run creates the administrator, who can then create a student") {
    Program p;
    std::string out = p.run(
        "admin\nSystem Admin\nS001\nadmin-pass\n"   // first run
        "1\nadmin\nadmin-pass\n"                    // log in
        "1\n1\n4\nann\nAnn Student\n2023001\npassword1\n"  // Users > Create > Student
        "0\n"                                       // log out
        "1\nann\npassword1\n0\n"                    // the student logs in and out
        "0\n");                                     // exit
    CHECK(contains(out, "Administrator created"));
    CHECK(contains(out, "User created."));
    CHECK(contains(out, "Welcome, Ann Student (Student)."));
    CHECK(contains(out, "Goodbye."));
}

TEST_CASE("NFR-09: wrong input and errors are shown, and the program keeps going") {
    Program p;
    p.admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
    std::string out = p.run(
        "9\nx\n"                       // not a menu choice
        "1\nadmin\nwrong-pass\n"       // wrong password
        "1\nadmin\nadmin-pass\n"
        "1\n1\n4\nadmin\nCopy\n2023001\npassword1\n"  // duplicate username
        "0\n0\n");
    CHECK(contains(out, "Please type a whole number from 0 to 1."));
    CHECK(contains(out, "! Wrong username or password"));
    CHECK(contains(out, "! The username admin is already taken"));
    CHECK(contains(out, "Goodbye."));
}

TEST_CASE("the program closes cleanly when the input ends in the middle of a menu") {
    Program p;
    p.admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
    std::string out = p.run("1\nadmin\nadmin-pass\n1\n");
    CHECK(contains(out, "Goodbye."));
}

TEST_CASE("a student sees their assignments and is told when a mark is not released") {
    Program p;
    p.admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
    auto s = p.auth.login("admin", "admin-pass");
    int hod = p.admin.createUser(s, domain::Role::HeadOfDepartment, "hod", "Mrs Head", "S100", "password1");
    int lect = p.admin.createUser(s, domain::Role::Lecturer, "lect", "Mr Teacher", "S200", "password1");
    int ann = p.admin.createUser(s, domain::Role::Student, "ann", "Ann Student", "2023001", "password1");
    int course = p.admin.createCourse(s, p.admin.createDepartment(s, "Computer Science", hod), "CS4433", "SE");
    p.admin.assignLecturer(s, course, lect);
    p.admin.enrolStudent(s, course, ann);
    p.assignments.createAssignment(p.auth.login("lect", "password1"), course, "Project", "", testutil::hoursAfter(48), 100);

    std::string out = p.run("1\nann\npassword1\n1\n3\n1\n0\n0\n");
    CHECK(contains(out, "Project, out of 100"));
    CHECK(contains(out, "Status: Not submitted"));
    CHECK(contains(out, "! You have not submitted this assignment"));
}

TEST_CASE("UC-09, UC-16, UC-20 through the menus with seeded data") {
    Program p;
    p.admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
    auto s = p.auth.login("admin", "admin-pass");
    int hod = p.admin.createUser(s, domain::Role::HeadOfDepartment, "hod", "Mrs Head", "S100", "password1");
    int lect = p.admin.createUser(s, domain::Role::Lecturer, "lect", "Mr Teacher", "S200", "password1");
    int ann = p.admin.createUser(s, domain::Role::Student, "ann", "Ann Student", "2023001", "password1");
    int ben = p.admin.createUser(s, domain::Role::Student, "ben", "Ben Student", "2023002", "password1");
    int course = p.admin.createCourse(s, p.admin.createDepartment(s, "Computer Science", hod), "CS4433", "SE");
    p.admin.assignLecturer(s, course, lect);
    p.admin.enrolStudent(s, course, ann);
    p.admin.enrolStudent(s, course, ben);
    p.assignments.createAssignment(p.auth.login("lect", "password1"), course, "Project", "", testutil::hoursAfter(48), 100);
    p.files.addSource("C:\\work\\ann.pdf", 2000);
    p.files.addSource("C:\\work\\ben.docx", 3000);

    // UC-09: Ann submits on time; Ben submits two hours after the deadline and is warned first.
    std::string out = p.run("1\nann\npassword1\n2\n1\nC:\\work\\ann.pdf\ny\n0\n0\n");
    CHECK(contains(out, "== Receipt =="));
    CHECK(contains(out, "Version 1"));
    p.clock.set(testutil::hoursAfter(50));
    out = p.run("1\nben\npassword1\n2\n1\nC:\\work\\ben.docx\ny\n0\n0\n");
    CHECK(contains(out, "This submission will be 1 day(s) late"));
    CHECK(contains(out, "== Receipt =="));

    // UC-16: the lecturer marks both; Ben's penalty is shown before saving.
    out = p.run("1\nlect\npassword1\n3\n1\n1\n"
                "1\n75\nWell structured\ny\n"
                "2\n80\nStrong but late\ny\n"
                "0\n0\n0\n");
    CHECK(contains(out, "Days late: 0   Penalty: 0   Final mark: 75"));
    CHECK(contains(out, "Days late: 1   Penalty: 10   Final mark: 70"));
    CHECK(contains(out, "Ben Student (2023002)  Awaiting moderation, 1 day(s) late, final 70"));

    // UC-20: the HoD approves Ann, adjusts Ben with a reason, and releases both.
    out = p.run("1\nhod\npassword1\n1\n1\n"
                "1\n1\n"
                "2\n2\n72\nPenalty agreed with lecturer\n"
                "0\n"
                "2\n1\n1\ny\n"
                "3\n1\n"
                "0\n0\n");
    CHECK(contains(out, "Mark approved."));
    CHECK(contains(out, "Mark adjusted."));
    CHECK(contains(out, "2 mark(s) released."));
    CHECK(contains(out, "Audit log (3 entries)"));
    CHECK(contains(out, "70 -> 72  by Mrs Head (S100)"));

    // FR-18: Ben now sees his released mark.
    out = p.run("1\nben\npassword1\n3\n1\n0\n0\n");
    CHECK(contains(out, "Raw mark: 80 out of 100"));
    CHECK(contains(out, "Late penalty: 10"));
    CHECK(contains(out, "Final mark: 72"));
}
