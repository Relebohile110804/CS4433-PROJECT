#include "doctest/doctest.h"

#include "../TestHelpers.h"
#include "business/domain/User.h"
#include "data/SqliteAssignmentRepository.h"
#include "data/SqliteAuditRepository.h"
#include "data/SqliteCourseRepository.h"
#include "data/SqliteSubmissionRepository.h"
#include "data/SqliteUserRepository.h"

using namespace lms::data;
using namespace lms::domain;
using testutil::hash;
using testutil::hoursAfter;

namespace {

// A small school: one department with its HoD, one lecturer, two students and one course.
struct School {
    Database db{":memory:"};
    SqliteUserRepository users{db};
    SqliteCourseRepository courses{db};
    SqliteAssignmentRepository assignments{db};
    SqliteSubmissionRepository submissions{db};
    SqliteAuditRepository audit{db};
    int hod = 0, lecturer = 0, student1 = 0, student2 = 0, dept = 0, course = 0;

    School() {
        hod = addUser(HeadOfDepartment(0, "hod", "Mrs Head", "S100"));
        lecturer = addUser(Lecturer(0, "lect", "Mr Teacher", "S200"));
        student1 = addUser(Student(0, "stu1", "Ann Student", "2023001"));
        student2 = addUser(Student(0, "stu2", "Ben Student", "2023002"));
        dept = courses.addDepartment("Computer Science", hod);
        course = courses.addCourse(dept, "CS4433", "Software Engineering");
        courses.addLecturer(course, lecturer);
        courses.enrol(course, student1);
    }

    int addUser(User&& u) {
        u.setPasswordHash("hash");
        return users.add(u);
    }

    int addAssignment() { return assignments.add(Assignment(0, course, "Project", "Build it", hoursAfter(48), 100)); }
};

SubmissionVersion version(int n) {
    return SubmissionVersion(n, "submissions/v" + std::to_string(n) + ".pdf", hash(), hoursAfter(n));
}

}  // namespace

TEST_CASE("users are saved and read back as the right subclass") {
    School s;
    auto hod = s.users.findById(s.hod);
    REQUIRE(hod);
    CHECK(hod->getRole() == Role::HeadOfDepartment);
    CHECK(hod->getPersonNumber() == "S100");
    CHECK(hod->getPasswordHash() == "hash");

    auto stu = s.users.findByUsername("stu1");
    REQUIRE(stu);
    CHECK(stu->getRole() == Role::Student);
    CHECK(stu->getId() == s.student1);
    CHECK(s.users.findByUsername("nobody") == nullptr);
    CHECK(s.users.personNumberExists("2023002"));
    CHECK_FALSE(s.users.personNumberExists("9999"));
}

TEST_CASE("usernames are unique") {
    School s;
    CHECK_THROWS_AS(s.addUser(Student(0, "stu1", "Copy", "2023999")), DatabaseError);
}

TEST_CASE("user update keeps lockout state and deactivation") {
    School s;
    auto u = s.users.findById(s.student1);
    for (int i = 0; i < User::MaxFailedLogins; ++i) u->recordFailedLogin(hoursAfter(0));
    u->deactivate();
    u->setFullName("Ann Renamed");
    s.users.update(*u);

    auto again = s.users.findById(s.student1);
    CHECK(again->isLocked(hoursAfter(0)));
    CHECK_FALSE(again->isActive());
    CHECK(again->getFullName() == "Ann Renamed");
}

TEST_CASE("list users by role") {
    School s;
    CHECK(s.users.list(std::nullopt).size() == 4);
    auto students = s.users.list(Role::Student);
    REQUIRE(students.size() == 2);
    CHECK(students[0]->getFullName() == "Ann Student");  // ordered by name
}

TEST_CASE("departments and courses") {
    School s;
    auto d = s.courses.departmentOfHod(s.hod);
    REQUIRE(d);
    CHECK(d->getName() == "Computer Science");
    CHECK_FALSE(s.courses.departmentOfHod(s.lecturer));

    d->setName("Computing");
    s.courses.updateDepartment(*d);
    CHECK(s.courses.findDepartment(s.dept)->getName() == "Computing");
    CHECK(s.courses.listDepartments().size() == 1);

    auto c = s.courses.findCourse(s.course);
    REQUIRE(c);
    CHECK(c->getCode() == "CS4433");
    CHECK(c->isTaughtBy(s.lecturer));
    CHECK(c->hasStudent(s.student1));
    CHECK(s.courses.departmentOf(s.course) == s.dept);

    c->rename("CS4434", "Advanced SE");
    s.courses.updateCourse(*c);
    CHECK(s.courses.findCourseByCode("CS4434"));
    CHECK_FALSE(s.courses.findCourseByCode("CS4433"));
    CHECK(s.courses.coursesInDepartment(s.dept).size() == 1);
    CHECK(s.courses.listCourses().size() == 1);
}

TEST_CASE("lecturers and enrolments") {
    School s;
    CHECK(s.courses.isTeaching(s.lecturer, s.course));
    CHECK(s.courses.coursesForLecturer(s.lecturer).size() == 1);
    s.courses.addLecturer(s.course, s.lecturer);  // adding twice does nothing
    s.courses.removeLecturer(s.course, s.lecturer);
    CHECK_FALSE(s.courses.isTeaching(s.lecturer, s.course));

    CHECK(s.courses.isEnrolled(s.student1, s.course));
    CHECK_FALSE(s.courses.isEnrolled(s.student2, s.course));
    s.courses.enrol(s.course, s.student2);
    CHECK(s.courses.coursesForStudent(s.student2).size() == 1);
    s.courses.withdraw(s.course, s.student2);
    CHECK(s.courses.coursesForStudent(s.student2).empty());
}

TEST_CASE("assignments with extensions") {
    School s;
    int id = s.addAssignment();
    s.assignments.addExtension(id, Extension(0, s.student1, hoursAfter(96), "Illness", s.lecturer, hoursAfter(1)));

    auto a = s.assignments.findById(id);
    REQUIRE(a);
    CHECK(a->getTitle() == "Project");
    CHECK(a->getMaxMark() == 100);
    CHECK(a->getDeadline() == hoursAfter(48));
    REQUIRE(a->getExtensions().size() == 1);
    CHECK(a->effectiveDeadlineFor(s.student1) == hoursAfter(96));
    CHECK(a->effectiveDeadlineFor(s.student2) == hoursAfter(48));
    CHECK(s.assignments.forCourse(s.course).size() == 1);
    CHECK_FALSE(s.assignments.findById(999));
}

TEST_CASE("submission life cycle is stored: versions, mark, moderation, release") {
    School s;
    int aId = s.addAssignment();

    Submission sub = Submission::createNew(aId, s.student1, version(1), 0);
    int id = s.submissions.save(sub);
    CHECK(id > 0);

    sub = *s.submissions.findById(id);
    sub.addVersion(version(2), 0);
    s.submissions.save(sub);

    sub = *s.submissions.find(aId, s.student1);
    REQUIRE(sub.getVersions().size() == 2);
    CHECK(sub.currentVersion().getStoredPath() == "submissions/v2.pdf");
    CHECK(sub.getStatus() == SubmissionStatus::Submitted);

    sub.recordMark(Mark(70, 0, 70, "Well done", s.lecturer, hoursAfter(50)));
    s.submissions.save(sub);
    sub = *s.submissions.findById(id);
    REQUIRE(sub.getMark());
    CHECK(sub.getMark()->getFinalMark() == doctest::Approx(70));
    CHECK(sub.getStatus() == SubmissionStatus::Marked);

    sub.recordMark(Mark(75, 0, 75, "Re-marked", s.lecturer, hoursAfter(51)));  // correction replaces the mark
    sub.adjust(72.5, s.hod, hoursAfter(52));
    sub.release(hoursAfter(53));
    s.submissions.save(sub);

    sub = *s.submissions.findById(id);
    CHECK(sub.getStatus() == SubmissionStatus::Released);
    CHECK(sub.getMark()->getRawMark() == 75);
    CHECK(sub.getMark()->getFinalMark() == doctest::Approx(72.5));
    CHECK(sub.getModeratedBy() == s.hod);
    CHECK(sub.getReleasedAt() == hoursAfter(53));
    CHECK(s.submissions.forAssignment(aId).size() == 1);
    CHECK(s.submissions.forStudent(s.student1).size() == 1);
    CHECK(s.submissions.forStudent(s.student2).empty());
}

TEST_CASE("returned submission keeps the HoD comment") {
    School s;
    int aId = s.addAssignment();
    Submission sub = Submission::createNew(aId, s.student1, version(1), 2);
    sub.recordMark(Mark(70, 20, 50, "Late but good", s.lecturer, hoursAfter(80)));
    sub.returnToLecturer("Check the penalty");
    int id = s.submissions.save(sub);

    sub = *s.submissions.findById(id);
    CHECK(sub.getStatus() == SubmissionStatus::Returned);
    CHECK(sub.getDaysLate() == 2);
    CHECK(sub.getHodComment() == "Check the penalty");
    CHECK(sub.getMark()->getPenalty() == doctest::Approx(20));
}

TEST_CASE("one submission per student per assignment") {
    School s;
    int aId = s.addAssignment();
    s.submissions.save(Submission::createNew(aId, s.student1, version(1), 0));
    CHECK_THROWS_AS(s.submissions.save(Submission::createNew(aId, s.student1, version(1), 0)), DatabaseError);
}

TEST_CASE("audit log query filters by department, course and assignment") {
    School s;
    int a1 = s.addAssignment();
    int a2 = s.addAssignment();
    int sub1 = s.submissions.save(Submission::createNew(a1, s.student1, version(1), 0));
    int sub2 = s.submissions.save(Submission::createNew(a2, s.student1,
                                                        SubmissionVersion(1, "submissions/other.pdf", hash('b'), hoursAfter(1)), 0));
    s.audit.append(AuditEntry(0, sub1, s.lecturer, hoursAfter(50), std::nullopt, 70, "initial marking"));
    s.audit.append(AuditEntry(0, sub1, s.hod, hoursAfter(60), 70.0, 65, "Too generous"));
    s.audit.append(AuditEntry(0, sub2, s.lecturer, hoursAfter(55), std::nullopt, 80, "initial marking"));

    auto all = s.audit.query(std::nullopt, std::nullopt, std::nullopt);
    REQUIRE(all.size() == 3);
    CHECK(all[0].getReason() == "initial marking");  // oldest first
    CHECK_FALSE(all[0].getOldValue().has_value());

    auto forA1 = s.audit.query(std::nullopt, std::nullopt, a1);
    REQUIRE(forA1.size() == 2);
    CHECK(*forA1[1].getOldValue() == doctest::Approx(70));
    CHECK(forA1[1].getChangedBy() == s.hod);

    CHECK(s.audit.query(s.dept, std::nullopt, std::nullopt).size() == 3);
    CHECK(s.audit.query(s.dept + 1, std::nullopt, std::nullopt).empty());
    CHECK(s.audit.query(std::nullopt, s.course, std::nullopt).size() == 3);
}
