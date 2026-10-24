// The three dressed use cases run end to end on the real SQLite database and
// file store: UC-09 Submit, UC-16 Mark, UC-20 Moderate and Release.
#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>

#include "../TestHelpers.h"
#include "../fakes/Fakes.h"
#include "business/domain/Errors.h"
#include "business/services/AssignmentService.h"
#include "business/services/AuditService.h"
#include "business/services/MarkingService.h"
#include "business/services/ModerationService.h"
#include "business/services/SubmissionService.h"
#include "business/services/TenPercentPerDayPolicy.h"
#include "business/services/UserFactory.h"
#include "data/Database.h"
#include "data/LocalFileStore.h"
#include "data/SqliteAssignmentRepository.h"
#include "data/SqliteAuditRepository.h"
#include "data/SqliteCourseRepository.h"
#include "data/SqliteSubmissionRepository.h"
#include "data/SqliteUserRepository.h"

using namespace lms::domain;
using namespace lms::services;
using testutil::hoursAfter;
namespace fs = std::filesystem;

namespace {

struct RealSchool {
    fs::path dir = fs::temp_directory_path() / "lms_workflow_test";
    lms::data::Database db{":memory:"};
    lms::data::SqliteUserRepository users{db};
    lms::data::SqliteCourseRepository courses{db};
    lms::data::SqliteAssignmentRepository assignmentRepo{db};
    lms::data::SqliteSubmissionRepository submissions{db};
    lms::data::SqliteAuditRepository auditRepo{db};
    std::unique_ptr<lms::data::LocalFileStore> files;
    fakes::FakeClock clock{testutil::base()};
    TenPercentPerDayPolicy policy;
    std::unique_ptr<AssignmentService> assignments;
    std::unique_ptr<AuditService> audit;
    std::unique_ptr<SubmissionService> submitting;
    std::unique_ptr<MarkingService> marking;
    std::unique_ptr<ModerationService> moderation;
    int hod, lecturer, ann, ben, course;

    RealSchool() {
        fs::remove_all(dir);
        fs::create_directories(dir);
        files = std::make_unique<lms::data::LocalFileStore>((dir / "submissions").string());
        assignments = std::make_unique<AssignmentService>(assignmentRepo, courses, clock);
        audit = std::make_unique<AuditService>(auditRepo, courses, clock);
        submitting = std::make_unique<SubmissionService>(submissions, assignmentRepo, courses, *files, policy, clock, db);
        marking = std::make_unique<MarkingService>(submissions, assignmentRepo, courses, policy, *audit, clock, db);
        moderation = std::make_unique<ModerationService>(submissions, assignmentRepo, courses, *audit, clock, db);

        hod = addUser(Role::HeadOfDepartment, "hod", "S100");
        lecturer = addUser(Role::Lecturer, "lect", "S200");
        ann = addUser(Role::Student, "ann", "2023001");
        ben = addUser(Role::Student, "ben", "2023002");
        int dept = courses.addDepartment("Computer Science", hod);
        course = courses.addCourse(dept, "CS4433", "Software Engineering");
        courses.addLecturer(course, lecturer);
        courses.enrol(course, ann);
        courses.enrol(course, ben);
    }
    ~RealSchool() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    int addUser(Role role, const std::string& username, const std::string& number) {
        auto u = UserFactory::create(role, 0, username, username, number);
        u->setPasswordHash("x");
        return users.add(*u);
    }
    Session as(int id) { return Session(std::shared_ptr<const User>(users.findById(id)), clock.now()); }
    std::string writeFile(const std::string& name, const std::string& text) {
        std::ofstream(dir / name, std::ios::binary) << text;
        return (dir / name).string();
    }
};

}  // namespace

TEST_CASE("UC-09, UC-16, UC-20 from submission to released mark") {
    RealSchool s;
    int a = s.assignments->createAssignment(s.as(s.lecturer), s.course, "Project", "Build it", hoursAfter(48), 100);

    // UC-09: Ann submits on time and once more before the deadline; Ben is a day and a bit late.
    s.clock.set(hoursAfter(20));
    s.submitting->submit(s.as(s.ann), a, s.writeFile("draft.pdf", "draft"));
    s.clock.set(hoursAfter(40));
    SubmissionVersion v2 = s.submitting->submit(s.as(s.ann), a, s.writeFile("final.pdf", "final"));
    CHECK(v2.getVersionNo() == 2);
    CHECK(fs::exists(v2.getStoredPath()));
    s.clock.set(hoursAfter(48 + 25));
    s.submitting->submit(s.as(s.ben), a, s.writeFile("ben.docx", "ben"));

    // UC-16: the lecturer sees both and marks them; Ben loses 2 days x 10.
    s.clock.set(hoursAfter(100));
    auto list = s.marking->listSubmissions(s.as(s.lecturer), a);
    REQUIRE(list.size() == 2);
    int annSub = s.submissions.find(a, s.ann)->getId();
    int benSub = s.submissions.find(a, s.ben)->getId();
    s.marking->mark(s.as(s.lecturer), annSub, 75, "Well structured", "");
    s.marking->mark(s.as(s.lecturer), benSub, 80, "Strong, but late", "");
    CHECK(s.submissions.findById(benSub)->getMark()->getFinalMark() == doctest::Approx(60));
    CHECK_THROWS_WITH(s.submitting->viewReleasedMark(s.as(s.ann), a), "Your mark has not been released yet");

    // UC-20: the HoD approves Ann, adjusts Ben, and releases both.
    Session hod = s.as(s.hod);
    REQUIRE(s.moderation->queue(hod).size() == 1);
    s.moderation->approve(hod, annSub);
    s.moderation->adjust(hod, benSub, 62, "Penalty rounding agreed with lecturer");
    CHECK(s.moderation->release(hod, a) == 2);

    CHECK(s.submitting->viewReleasedMark(s.as(s.ann), a).getFinalMark() == doctest::Approx(75));
    CHECK(s.submitting->viewReleasedMark(s.as(s.ben), a).getFinalMark() == doctest::Approx(62));

    // FR-19, FR-20: two first marks and one adjustment are in the audit log.
    auto log = s.audit->viewLog(hod, s.course, a);
    REQUIRE(log.size() == 3);
    CHECK(log[2].getReason() == "Penalty rounding agreed with lecturer");
    CHECK(*log[2].getOldValue() == doctest::Approx(60));
}

TEST_CASE("BR-07: a failed audit write rolls back the mark in SQLite") {
    RealSchool s;
    int a = s.assignments->createAssignment(s.as(s.lecturer), s.course, "Project", "Build it", hoursAfter(48), 100);
    s.clock.set(hoursAfter(10));
    s.submitting->submit(s.as(s.ann), a, s.writeFile("work.txt", "work"));
    int id = s.submissions.find(a, s.ann)->getId();

    // An audit service that always fails, standing in for a database error part-way through.
    struct BrokenAudit : IAuditService {
        void record(int, int, std::optional<double>, double, const std::string&) override {
            throw std::runtime_error("audit write failed");
        }
        std::vector<AuditEntry> viewLog(const Session&, std::optional<int>, std::optional<int>) override { return {}; }
    } broken;
    MarkingService marking(s.submissions, s.assignmentRepo, s.courses, s.policy, broken, s.clock, s.db);
    CHECK_THROWS(marking.mark(s.as(s.lecturer), id, 70, "Good", ""));

    auto sub = s.submissions.findById(id);
    CHECK_FALSE(sub->isMarked());
    CHECK(sub->getStatus() == SubmissionStatus::Submitted);
}
