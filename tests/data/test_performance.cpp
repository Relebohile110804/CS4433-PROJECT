// NFR-01 to NFR-03 and R8: the list screens are timed on a database seeded to the full
// school size from the SRS: 800 students, 60 staff, 100 courses, 500 assignments and
// 20,000 submissions. Each screen must answer within 1 second (NFR-02).
#include "catch2/catch_amalgamated.hpp"

#include <chrono>
#include <filesystem>
#include <algorithm>
#include <functional>

#include "../TestHelpers.h"
#include "../fakes/Fakes.h"
#include "business/services/AssignmentService.h"
#include "business/services/AuditService.h"
#include "business/services/MarkingService.h"
#include "business/services/ModerationService.h"
#include "business/services/TenPercentPerDayPolicy.h"
#include "business/services/UserFactory.h"
#include "data/Database.h"
#include "data/SqliteAssignmentRepository.h"
#include "data/SqliteAuditRepository.h"
#include "data/SqliteCourseRepository.h"
#include "data/SqliteSubmissionRepository.h"
#include "data/SqliteUserRepository.h"

using namespace lms;
using namespace lms::domain;
using testutil::hoursAfter;

namespace {

double millisecondsFor(const std::function<void()>& work) {
    auto start = std::chrono::steady_clock::now();
    work();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

TEST_CASE("NFR-02, NFR-03: list screens answer within 1 second at full school size", "[system][performance]") {
    // A real file, not an in-memory database, so the disk is part of the timing.
    std::filesystem::path file = std::filesystem::temp_directory_path() / "lms_performance_test.db";
    std::filesystem::remove(file);
    // Declared before the database so it runs after the database is closed: Windows cannot
    // delete a file that is still open (defect found on the development laptop).
    struct RemoveAtEnd {
        std::filesystem::path path;
        ~RemoveAtEnd() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } cleanup{file};
    data::Database db(file.string());
    data::SqliteUserRepository users(db);
    data::SqliteCourseRepository courses(db);
    data::SqliteAssignmentRepository assignmentRepo(db);
    data::SqliteSubmissionRepository submissions(db);
    data::SqliteAuditRepository auditRepo(db);
    fakes::FakeClock clock{hoursAfter(1000)};
    services::TenPercentPerDayPolicy policy;
    services::AssignmentService assignments(assignmentRepo, courses, clock);
    services::AuditService audit(auditRepo, courses, clock);
    services::MarkingService marking(submissions, assignmentRepo, courses, policy, audit, clock, db);
    services::ModerationService moderation(submissions, assignmentRepo, courses, audit, clock, db);

    auto addUser = [&](Role role, const std::string& name) {
        auto u = services::UserFactory::create(role, 0, name, name, name);
        u->setPasswordHash("x");
        return users.add(*u);
    };

    // 5 departments x 20 courses = 100 courses; 5 HoDs + 54 lecturers + 1 admin = 60 staff;
    // each student takes 5 courses (40 students per course); 5 assignments per course = 500;
    // every student submits every assignment = 20,000 submissions, and 2,000 of them are marked.
    std::vector<int> hods, lecturers, students, courseIds, assignmentIds;
    int markedCount = 0;
    double seedMs = millisecondsFor([&] {
        auto t = db.begin();
        addUser(Role::Administrator, "admin");
        for (int i = 0; i < 5; ++i) hods.push_back(addUser(Role::HeadOfDepartment, "hod" + std::to_string(i)));
        for (int i = 0; i < 54; ++i) lecturers.push_back(addUser(Role::Lecturer, "lect" + std::to_string(i)));
        for (int i = 0; i < 800; ++i) students.push_back(addUser(Role::Student, "s" + std::to_string(i)));
        for (int d = 0; d < 5; ++d) {
            int dept = courses.addDepartment("Department " + std::to_string(d), hods[d]);
            for (int c = 0; c < 20; ++c) {
                int n = d * 20 + c;
                int course = courses.addCourse(dept, "C" + std::to_string(n), "Course " + std::to_string(n));
                courseIds.push_back(course);
                courses.addLecturer(course, lecturers[n % lecturers.size()]);
                for (int k = 0; k < 40; ++k) courses.enrol(course, students[(n * 40 + k) % 800]);
                for (int a = 0; a < 5; ++a)
                    assignmentIds.push_back(
                        assignmentRepo.add(Assignment(0, course, "Task " + std::to_string(a), "", hoursAfter(48), 100)));
            }
        }
        for (int aId : assignmentIds) {
            Assignment a = *assignmentRepo.findById(aId);
            int n = static_cast<int>(std::find(courseIds.begin(), courseIds.end(), a.getCourseId()) - courseIds.begin());
            for (int k = 0; k < 40; ++k) {
                int student = students[(n * 40 + k) % 800];
                SubmissionVersion v(1, "submissions/a" + std::to_string(aId) + "_s" + std::to_string(student) + "_v1.pdf",
                                    testutil::hash(), hoursAfter(24));
                Submission sub = Submission::createNew(aId, student, v, 0);
                if (k < 4) {
                    sub.recordMark(Mark(70, 0, 70, "Good", lecturers[0], hoursAfter(100)));
                    ++markedCount;
                }
                int id = submissions.save(sub);
                if (k < 4) auditRepo.append(AuditEntry(0, id, lecturers[0], hoursAfter(100), std::nullopt, 70, "First mark"));
            }
        }
        t->commit();
    });
    WARN("Seeding 20,000 submissions took " << seedMs << " ms");
    REQUIRE(markedCount == 2000);

    auto session = [&](int id) { return Session(std::shared_ptr<const User>(users.findById(id)), clock.now()); };
    Session lect = session(lecturers[0]);
    Session hod = session(hods[0]);
    Session student = session(students[0]);
    Session admin = session(1);

    double worst = 0;
    auto timeScreen = [&](const std::string& name, const std::function<void()>& work) {
        double ms = millisecondsFor(work);
        WARN(name << ": " << ms << " ms");
        worst = std::max(worst, ms);
    };
    timeScreen("Find user at login", [&] { CHECK(users.findByUsername("s799")); });
    timeScreen("Lecturer submissions list", [&] { CHECK(marking.listSubmissions(lect, assignmentIds[0]).size() == 40); });
    timeScreen("Student assignments list", [&] { CHECK(assignments.listForStudent(student).size() == 25); });
    timeScreen("HoD moderation queue", [&] { CHECK(moderation.queue(hod).size() == 100); });
    timeScreen("HoD submissions for moderation", [&] { CHECK(moderation.listForModeration(hod, assignmentIds[0]).size() == 4); });
    timeScreen("Audit log, whole school", [&] { CHECK(audit.viewLog(admin, std::nullopt, std::nullopt).size() == 2000); });
    timeScreen("Audit log, one department", [&] { CHECK(audit.viewLog(hod, std::nullopt, std::nullopt).size() == 400); });
    CHECK(worst < 1000.0);
}
