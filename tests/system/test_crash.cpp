// NFR-08: the marking process is killed at random moments, 100 times, while it is saving
// marks. Afterwards the database must not be damaged and every stored mark must match its
// latest audit entry, so no mark change was ever kept without its audit entry (BR-07).
// Killing a child process needs fork(), so this test runs on Linux (CI) only.
#include "SystemFixture.h"

#ifndef _WIN32
#include <random>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace systest;

TEST_CASE("ST-45: NFR-08 killing the program while it saves marks leaves no mark without its audit entry",
          "[system]") {
#ifdef _WIN32
    SKIP("Needs fork() and SIGKILL; runs in CI on Linux");
#else
    TempDir dir;
    std::string file = (dir.path / "lms.db").string();
    std::vector<int> submissionIds;
    {
        data::Database db(file);
        data::SqliteUserRepository users(db);
        data::SqliteCourseRepository courses(db);
        data::SqliteAssignmentRepository assignmentRepo(db);
        data::SqliteSubmissionRepository submissionRepo(db);
        fakes::FakePasswordHasher hasher;
        fakes::FakeClock clock{testutil::base()};
        services::AuthService auth(users, hasher, clock);
        services::AdminService admin(users, courses, hasher, db);
        services::AssignmentService assignments(assignmentRepo, courses, clock);
        admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
        Session s = auth.login("admin", "admin-pass");
        int hod = admin.createUser(s, Role::HeadOfDepartment, "hod", "Mrs Head", "S100", "password1");
        int lect = admin.createUser(s, Role::Lecturer, "lect", "Mr Teacher", "S200", "password1");
        int course = admin.createCourse(s, admin.createDepartment(s, "Computer Science", hod), "CS4433", "SE");
        admin.assignLecturer(s, course, lect);
        int project = assignments.createAssignment(auth.login("lect", "password1"), course, "Project", "",
                                                   hoursAfter(48), 100);
        for (int i = 0; i < 20; ++i) {
            int student = admin.createUser(s, Role::Student, "s" + std::to_string(i), "Student " + std::to_string(i),
                                           "2023" + std::to_string(100 + i), "password1");
            admin.enrolStudent(s, course, student);
            SubmissionVersion v(1, "submissions/s" + std::to_string(i) + ".pdf", testutil::hash(), hoursAfter(1));
            submissionIds.push_back(submissionRepo.save(Submission::createNew(project, student, v, 0)));
        }
    }

    std::mt19937 random(20261103);
    std::uniform_int_distribution<int> delayMicros(2000, 40000);
    int midTransaction = 0;  // kills that left a half-written transaction behind
    for (int round = 0; round < 100; ++round) {
        pid_t child = fork();
        REQUIRE(child >= 0);
        if (child == 0) {
            // The child is the program marking work, over and over, until it is killed.
            data::Database db(file);
            data::SqliteUserRepository users(db);
            data::SqliteCourseRepository courses(db);
            data::SqliteAssignmentRepository assignmentRepo(db);
            data::SqliteSubmissionRepository submissionRepo(db);
            data::SqliteAuditRepository auditRepo(db);
            fakes::FakePasswordHasher hasher;
            fakes::FakeClock clock{hoursAfter(100)};
            services::TenPercentPerDayPolicy policy;
            services::AuthService auth(users, hasher, clock);
            services::AuditService audit(auditRepo, courses, clock);
            services::MarkingService marking(submissionRepo, assignmentRepo, courses, policy, audit, clock, db);
            Session lect = auth.login("lect", "password1");
            for (int i = 0; i < 100000; ++i)
                marking.mark(lect, submissionIds[static_cast<std::size_t>(i) % submissionIds.size()],
                             (round * 7 + i) % 101, "Feedback " + std::to_string(i), "Re-mark " + std::to_string(i));
            _exit(0);
        }
        usleep(static_cast<useconds_t>(delayMicros(random)));
        kill(child, SIGKILL);
        int status = 0;
        waitpid(child, &status, 0);
        std::error_code ec;
        if (fs::exists(file + "-journal", ec) && fs::file_size(file + "-journal", ec) > 0) ++midTransaction;
    }

    // Open the database as the program would after a crash (SQLite rolls back any half-done
    // transaction) and check it.
    data::Database db(file);
    auto check = db.prepare("PRAGMA integrity_check");
    REQUIRE(check.step());
    CHECK(check.columnText(0) == "ok");
    auto orphan = db.prepare(
        "SELECT count(*) FROM marks m WHERE m.final_mark IS NOT (SELECT a.new_value FROM audit_log a "
        "WHERE a.submission_id = m.submission_id ORDER BY a.audit_id DESC LIMIT 1)");
    REQUIRE(orphan.step());
    CHECK(orphan.columnInt(0) == 0);
    auto noMark = db.prepare(
        "SELECT count(*) FROM audit_log a WHERE NOT EXISTS (SELECT 1 FROM marks m WHERE m.submission_id = a.submission_id)");
    REQUIRE(noMark.step());
    CHECK(noMark.columnInt(0) == 0);
    auto total = db.prepare("SELECT count(*) FROM audit_log");
    REQUIRE(total.step());
    WARN("NFR-08: 100 kills, " << midTransaction << " of them in the middle of a transaction; "
                                << total.columnInt(0) << " mark changes were saved, each with its audit entry");
    CHECK(total.columnInt(0) > 0);
#endif
}
