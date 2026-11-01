// One test for each equivalence partition (EP) and boundary value (BV) case in
// Section 1.4 of the Phase 5 test plan. The test name starts with the case ID.
#include "catch2/catch_amalgamated.hpp"

#include <sstream>

#include "../services/ServiceFixture.h"
#include "business/domain/Errors.h"
#include "business/services/AdminService.h"
#include "business/services/AssignmentService.h"
#include "business/services/AuthService.h"
#include "presentation/ConsoleIO.h"

using namespace lms::domain;
using lms::presentation::ConsoleIO;
using lms::services::AssignmentService;
using lms::services::AuthService;
using lms::services::SubmissionService;
using lms::services::TenPercentPerDayPolicy;
using testutil::hoursAfter;
using std::chrono::seconds;

namespace {

const TimePoint D = hoursAfter(48);  // the Project deadline in ServiceFixture

// Days late and final mark for 70 out of 100 handed in at the given time.
std::pair<int, double> penaltyAt(TimePoint submittedAt) {
    TenPercentPerDayPolicy p;
    int days = p.daysLate(D, submittedAt);
    return {days, p.finalMark(70, 100, days)};
}

// The fixture's student submits work.pdf at exactly time t.
int submitAtTime(ServiceFixture& f, TimePoint t, int who = 0) {
    int s = who ? who : f.student;
    f.clock.set(t);
    f.submitting.submit(f.as(s), f.assignment, "work.pdf");
    return f.submissions.find(f.assignment, s)->getId();
}

}  // namespace

// ---- Late penalty (BR-01, FR-13) ----

TEST_CASE("BV-01: one second before the deadline is on time", "[unit][plan]") {
    CHECK(penaltyAt(D - seconds(1)) == std::pair<int, double>{0, 70});
}
TEST_CASE("BV-02: exactly on the deadline is on time", "[unit][plan]") {
    CHECK(penaltyAt(D) == std::pair<int, double>{0, 70});
}
TEST_CASE("BV-03: one second late is 1 day late", "[unit][plan]") {
    CHECK(penaltyAt(D + seconds(1)) == std::pair<int, double>{1, 60});
}
TEST_CASE("BV-04: 24 hours late is still 1 day late", "[unit][plan]") {
    CHECK(penaltyAt(D + std::chrono::hours(24)) == std::pair<int, double>{1, 60});
}
TEST_CASE("BV-05: 24 hours and 1 second late is 2 days late", "[unit][plan]") {
    CHECK(penaltyAt(D + std::chrono::hours(24) + seconds(1)) == std::pair<int, double>{2, 50});
}
TEST_CASE("BV-06: 48 hours late is 2 days late", "[unit][plan]") {
    CHECK(penaltyAt(D + std::chrono::hours(48)) == std::pair<int, double>{2, 50});
}
TEST_CASE("BV-07: 48 hours and 1 second late is 3 days late", "[unit][plan]") {
    CHECK(penaltyAt(D + std::chrono::hours(48) + seconds(1)) == std::pair<int, double>{3, 40});
}
TEST_CASE("BV-08: 72 hours late is 3 days late, the last day with a penalty", "[unit][plan]") {
    CHECK(penaltyAt(D + std::chrono::hours(72)) == std::pair<int, double>{3, 40});
}
TEST_CASE("BV-09: 72 hours and 1 second late gives a final mark of 0", "[unit][plan]") {
    CHECK(penaltyAt(D + std::chrono::hours(72) + seconds(1)) == std::pair<int, double>{4, 0});
}

TEST_CASE("EP-01: the final mark is never negative", "[unit][plan]") {
    CHECK(TenPercentPerDayPolicy().finalMark(15, 50, 3) == Catch::Approx(0));
}
TEST_CASE("EP-02: the penalty is 10% of the maximum mark, not of the raw mark", "[unit][plan]") {
    CHECK(TenPercentPerDayPolicy().finalMark(40, 50, 1) == Catch::Approx(35));
}

TEST_CASE("EP-03: a student with an extension who submits at the new deadline is on time", "[unit][plan]") {
    ServiceFixture f;
    TimePoint e = hoursAfter(96);
    f.assignments.addExtension(f.assignment, Extension(0, f.student, e, "Sick", f.lecturer, hoursAfter(1)));
    int id = submitAtTime(f, e);
    CHECK(f.submissions.findById(id)->getStatus() == SubmissionStatus::Submitted);
    CHECK(f.marking.previewMark(f.as(f.lecturer), id, 70).daysLate == 0);
}
TEST_CASE("EP-04: one second after the extended deadline is 1 day late", "[unit][plan]") {
    ServiceFixture f;
    TimePoint e = hoursAfter(96);
    f.assignments.addExtension(f.assignment, Extension(0, f.student, e, "Sick", f.lecturer, hoursAfter(1)));
    int id = submitAtTime(f, e + seconds(1));
    CHECK(f.submissions.findById(id)->getStatus() == SubmissionStatus::Late);
    CHECK(f.marking.previewMark(f.as(f.lecturer), id, 70).finalMark == Catch::Approx(60));
}
TEST_CASE("EP-05: an extension granted after a late submission, before marking, removes the penalty", "[unit][plan]") {
    ServiceFixture f;
    int id = submitAtTime(f, D + std::chrono::hours(30));  // 2 days late
    CHECK(f.marking.previewMark(f.as(f.lecturer), id, 70).finalMark == Catch::Approx(50));
    f.assignments.addExtension(f.assignment,
                               Extension(0, f.student, D + std::chrono::hours(40), "Sick", f.lecturer, f.clock.now()));
    CHECK(f.marking.previewMark(f.as(f.lecturer), id, 70).finalMark == Catch::Approx(70));
}

// ---- Other inputs ----

TEST_CASE("BV-10: raw mark must be 0 to the maximum mark", "[unit][plan]") {
    ServiceFixture f;
    int id = submitAtTime(f, hoursAfter(10));
    Session lect = f.as(f.lecturer);
    CHECK_THROWS_WITH(f.marking.previewMark(lect, id, -1), "The raw mark must be from 0 to 100");
    CHECK(f.marking.previewMark(lect, id, 0).finalMark == Catch::Approx(0));
    CHECK(f.marking.previewMark(lect, id, 100).finalMark == Catch::Approx(100));
    CHECK_THROWS_WITH(f.marking.previewMark(lect, id, 101), "The raw mark must be from 0 to 100");

    // A raw mark that is not a whole number is asked for again by the menu.
    std::istringstream in("abc\n70\n");
    std::ostringstream out;
    CHECK(ConsoleIO(in, out).number("Raw mark", 0, 100) == 70);
}

TEST_CASE("BV-11: maximum mark must be 1 to 1000", "[unit][plan]") {
    auto make = [](int max) { return Assignment(0, 1, "Task", "", hoursAfter(48), max); };
    CHECK_THROWS_AS(make(0), RuleViolation);
    CHECK_NOTHROW(make(1));
    CHECK_NOTHROW(make(1000));
    CHECK_THROWS_AS(make(1001), RuleViolation);
}

namespace {
struct OneUser {
    fakes::FakeUserRepository users;
    fakes::FakePasswordHasher hasher;
    fakes::FakeClock clock{testutil::base()};
    AuthService auth{users, hasher, clock};
    OneUser() {
        auto u = lms::services::UserFactory::create(Role::Student, 0, "ann", "Ann", "2023001");
        u->setPasswordHash(hasher.hash("password1"));
        users.add(*u);
    }
};
}  // namespace

TEST_CASE("BV-12: a new password needs at least 8 characters and must differ", "[unit][plan]") {
    OneUser f;
    Session s = f.auth.login("ann", "password1");
    CHECK_THROWS_WITH(f.auth.changePassword(s, "password1", "1234567"), "The password must be at least 8 characters");
    CHECK_THROWS_WITH(f.auth.changePassword(s, "password1", "password1"), "The new password must be different");
    CHECK_NOTHROW(f.auth.changePassword(s, "password1", "12345678"));
}

TEST_CASE("BV-13: four failed logins leave the account open, the fifth locks it", "[unit][plan]") {
    OneUser f;
    for (int i = 0; i < 4; ++i) CHECK_THROWS_WITH(f.auth.login("ann", "wrong"), "Wrong username or password");
    CHECK_NOTHROW(f.auth.login("ann", "password1"));  // 4 failures: still open
    for (int i = 0; i < 4; ++i) CHECK_THROWS(f.auth.login("ann", "wrong"));
    CHECK_THROWS_WITH(f.auth.login("ann", "wrong"), "Too many failed logins. This account is locked for 15 minutes");
}

TEST_CASE("BV-14: the lock lasts exactly 15 minutes", "[unit][plan]") {
    OneUser f;
    for (int i = 0; i < 5; ++i) CHECK_THROWS(f.auth.login("ann", "wrong"));
    f.clock.advance(std::chrono::minutes(14) + seconds(59));
    CHECK_THROWS_WITH(f.auth.login("ann", "password1"),
                      "This account is locked after too many failed logins. Try again in 15 minutes");
    f.clock.advance(seconds(1));  // exactly 15:00 after the lock
    CHECK_NOTHROW(f.auth.login("ann", "password1"));
}

TEST_CASE("BV-15: a file of exactly 10 MB is accepted, one byte more is refused", "[unit][plan]") {
    ServiceFixture f;
    f.clock.set(hoursAfter(10));
    f.files.addSource("max.zip", 10485760);
    f.files.addSource("over.zip", 10485761);
    CHECK_THROWS_WITH(f.submitting.submit(f.as(f.student), f.assignment, "over.zip"), "The file is larger than 10 MB");
    CHECK_NOTHROW(f.submitting.submit(f.as(f.student), f.assignment, "max.zip"));
}

TEST_CASE("EP-06: only the six allowed file types, in any letter case, from a file that exists", "[unit][plan]") {
    for (const char* ok : {"Report.PDF", "essay.docx", "notes.txt", "code.zip", "main.cpp", "lms.h"})
        CHECK(SubmissionService::isAllowedFileType(ok));
    for (const char* bad : {"notes.doc", "photo.jpg", "README", "folder.pdf/readme"})
        CHECK_FALSE(SubmissionService::isAllowedFileType(bad));

    ServiceFixture f;
    f.clock.set(hoursAfter(10));
    CHECK_THROWS_WITH(f.submitting.submit(f.as(f.student), f.assignment, "missing.pdf"),
                      "The file cannot be found or opened: missing.pdf");
}

TEST_CASE("BV-16: an extension must be later than the current deadline and in the future", "[unit][plan]") {
    ServiceFixture f;
    AssignmentService service(f.assignments, f.courses, f.clock);
    Session lect = f.as(f.lecturer);
    CHECK_THROWS_WITH(service.grantExtension(lect, f.assignment, f.student, D, "Sick"),
                      "The new deadline must be later than the student's current deadline");
    CHECK_NOTHROW(service.grantExtension(lect, f.assignment, f.student, D + seconds(1), "Sick"));

    // After the deadline has passed, a new deadline that is already in the past is refused.
    f.clock.set(D + std::chrono::hours(10));
    CHECK_THROWS_WITH(service.grantExtension(lect, f.assignment, f.student2, D + std::chrono::hours(5), "Sick"),
                      "The new deadline must be in the future");
}

TEST_CASE("BV-17: resubmission is allowed at the deadline and refused one second later", "[unit][plan]") {
    ServiceFixture f;
    submitAtTime(f, hoursAfter(10));
    f.clock.set(D);
    CHECK(f.submitting.submit(f.as(f.student), f.assignment, "work.pdf").getVersionNo() == 2);
    f.clock.set(D + seconds(1));
    CHECK_THROWS_WITH(f.submitting.submit(f.as(f.student), f.assignment, "work.pdf"),
                      "The deadline has passed, so the version already submitted stands (BR-02)");
}

TEST_CASE("BV-18: an assignment deadline must be later than now", "[unit][plan]") {
    ServiceFixture f;
    AssignmentService service(f.assignments, f.courses, f.clock);
    Session lect = f.as(f.lecturer);
    TimePoint now = f.clock.now();
    CHECK_THROWS_WITH(service.createAssignment(lect, f.course, "Quiz", "", now, 10), "The deadline must be in the future");
    CHECK_NOTHROW(service.createAssignment(lect, f.course, "Quiz", "", now + seconds(1), 10));
}

TEST_CASE("BV-19: an HoD adjusted mark must be 0 to the maximum mark", "[unit][plan]") {
    auto adjustTo = [](double mark) {
        ServiceFixture f;
        int id = f.markedSubmission(70);
        f.moderation.adjust(f.as(f.hod), id, mark, "Moderation");
        return f.submissions.findById(id)->getMark()->getFinalMark();
    };
    CHECK_THROWS_WITH(adjustTo(-0.5), "The new mark must be from 0 to 100");
    CHECK(adjustTo(0) == Catch::Approx(0));
    CHECK(adjustTo(100) == Catch::Approx(100));
    CHECK_THROWS_WITH(adjustTo(100.5), "The new mark must be from 0 to 100");
}

TEST_CASE("BV-20: a menu choice outside 0 to 4 is asked for again", "[unit][plan]") {
    for (const char* good : {"0", "4"}) {
        std::istringstream in(std::string(good) + "\n");
        std::ostringstream out;
        CHECK(ConsoleIO(in, out).number("Choose", 0, 4) == std::stoi(good));
    }
    std::istringstream in("-1\n5\nx\n\n3\n");
    std::ostringstream out;
    CHECK(ConsoleIO(in, out).number("Choose", 0, 4) == 3);
    std::string text = out.str();
    std::size_t count = 0;
    for (std::size_t at = text.find("Please type"); at != std::string::npos; at = text.find("Please type", at + 1)) ++count;
    CHECK(count == 4);  // -1, 5, x and the empty line were each refused
}

TEST_CASE("EP-07: only real dates in the YYYY-MM-DD HH:MM format are accepted", "[unit][plan]") {
    CHECK(ConsoleIO::parseDateTime("2028-02-29 00:00"));
    for (const char* bad : {"2027-02-29 00:00", "2026-13-01 10:00", "2026-11-31 10:00", "2026-11-07 24:00", "7/11/2026 17:00"})
        CHECK_FALSE(ConsoleIO::parseDateTime(bad));
}

TEST_CASE("EP-08: every wrong role is refused by every service", "[unit][plan]") {
    ServiceFixture f;
    int id = f.markedSubmission();
    AssignmentService assignments(f.assignments, f.courses, f.clock);
    fakes::FakeBackup backup;
    fakes::FakePasswordHasher hasher;
    lms::services::AdminService admin(f.users, f.courses, hasher, backup);
    int otherLecturer = f.addUser(Role::Lecturer, "lect2", "S201");
    int outsider = f.addUser(Role::Student, "out", "2023999");

    // Administration: only the administrator.
    for (int who : {f.hod, f.lecturer, f.student})
        CHECK_THROWS_AS(admin.createUser(f.as(who), Role::Student, "x", "X", "9", "password1"), AccessDenied);
    // Assignments and extensions: only a lecturer of the course.
    for (int who : {f.admin, f.hod, f.student, otherLecturer}) {
        CHECK_THROWS_AS(assignments.createAssignment(f.as(who), f.course, "Q", "", hoursAfter(90), 10), AccessDenied);
        CHECK_THROWS_AS(assignments.grantExtension(f.as(who), f.assignment, f.student, hoursAfter(90), "r"), AccessDenied);
    }
    // Submitting: only an enrolled student.
    for (int who : {f.admin, f.hod, f.lecturer, outsider})
        CHECK_THROWS_AS(f.submitting.submit(f.as(who), f.assignment, "work.pdf"), AccessDenied);
    // Marking: only a lecturer of the course.
    for (int who : {f.admin, f.hod, f.student, otherLecturer})
        CHECK_THROWS_AS(f.marking.mark(f.as(who), id, 50, "OK", "r"), AccessDenied);
    // Moderation and release: only the HoD of the department.
    for (int who : {f.admin, f.lecturer, f.student, f.otherHod}) {
        CHECK_THROWS_AS(f.moderation.approve(f.as(who), id), AccessDenied);
        CHECK_THROWS_AS(f.moderation.release(f.as(who), f.assignment), AccessDenied);
    }
    // Audit log: only the administrator and HoDs.
    for (int who : {f.lecturer, f.student})
        CHECK_THROWS_AS(f.audit.viewLog(f.as(who), std::nullopt, std::nullopt), AccessDenied);
    // Released marks: only the student who owns them (others have no submission to see).
    CHECK_THROWS_AS(f.submitting.viewReleasedMark(f.as(outsider), f.assignment), AccessDenied);
}
