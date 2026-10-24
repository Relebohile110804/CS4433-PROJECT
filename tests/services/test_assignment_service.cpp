#include "doctest/doctest.h"

#include "../TestHelpers.h"
#include "../fakes/FakeRepositories.h"
#include "../fakes/Fakes.h"
#include "business/domain/Errors.h"
#include "business/services/AssignmentService.h"

using namespace lms::domain;
using lms::services::AssignmentService;
using lms::services::UserFactory;
using testutil::hoursAfter;

namespace {

// One department with two courses. The lecturer teaches CS4433 only and the
// student is enrolled in CS4433 only. The clock starts at base().
struct AssignmentFixture {
    fakes::FakeUserRepository users;
    fakes::FakeCourseRepository courses;
    fakes::FakeAssignmentRepository assignments;
    fakes::FakeClock clock{testutil::base()};
    AssignmentService service{assignments, courses, clock};
    int hod, lecturer, student, otherStudent, dept, course, otherCourse;

    AssignmentFixture() {
        hod = addUser(Role::HeadOfDepartment, "hod", "S100");
        lecturer = addUser(Role::Lecturer, "lect", "S200");
        student = addUser(Role::Student, "stu", "2023001");
        otherStudent = addUser(Role::Student, "stu2", "2023002");
        dept = courses.addDepartment("Computer Science", hod);
        course = courses.addCourse(dept, "CS4433", "Software Engineering");
        otherCourse = courses.addCourse(dept, "CS1000", "Intro");
        courses.addLecturer(course, lecturer);
        courses.enrol(course, student);
        courses.enrol(otherCourse, otherStudent);
    }
    int addUser(Role role, const std::string& username, const std::string& number) {
        auto u = UserFactory::create(role, 0, username, username, number);
        u->setPasswordHash("x");
        return users.add(*u);
    }
    Session as(int userId) { return Session(std::shared_ptr<const User>(users.findById(userId)), clock.now()); }
    // An assignment due 48 hours from the start.
    int project() { return service.createAssignment(as(lecturer), course, "Project", "Build the LMS", hoursAfter(48), 100); }
};

}  // namespace

TEST_CASE("FR-08: a lecturer creates an assignment for a course they teach") {
    AssignmentFixture f;
    int id = f.project();
    auto a = f.assignments.findById(id);
    REQUIRE(a);
    CHECK(a->getTitle() == "Project");
    CHECK(a->getMaxMark() == 100);
    CHECK(a->getDeadline() == hoursAfter(48));
}

TEST_CASE("FR-08: assignment rules") {
    AssignmentFixture f;
    Session lect = f.as(f.lecturer);
    CHECK_THROWS_AS(f.service.createAssignment(lect, f.otherCourse, "Quiz", "", hoursAfter(48), 10), AccessDenied);
    CHECK_THROWS_AS(f.service.createAssignment(f.as(f.student), f.course, "Quiz", "", hoursAfter(48), 10),
                    AccessDenied);
    CHECK_THROWS_WITH(f.service.createAssignment(lect, 999, "Quiz", "", hoursAfter(48), 10), "Course not found");
    CHECK_THROWS_WITH(f.service.createAssignment(lect, f.course, "Quiz", "", testutil::base(), 10),
                      "The deadline must be in the future");
    CHECK_THROWS_WITH(f.service.createAssignment(lect, f.course, "Quiz", "", hoursAfter(48), 0),
                      "Maximum mark must be a whole number from 1 to 1000");
    CHECK_THROWS_WITH(f.service.createAssignment(lect, f.course, " ", "", hoursAfter(48), 10),
                      "Assignment title cannot be empty");
}

TEST_CASE("FR-09, BR-03: an extension changes only that student's deadline") {
    AssignmentFixture f;
    int a = f.project();
    f.courses.enrol(f.course, f.otherStudent);

    f.service.grantExtension(f.as(f.lecturer), a, f.student, hoursAfter(96), "Medical certificate");
    CHECK(f.service.effectiveDeadline(a, f.student) == hoursAfter(96));
    CHECK(f.service.effectiveDeadline(a, f.otherStudent) == hoursAfter(48));

    auto stored = f.assignments.findById(a);
    REQUIRE(stored->getExtensions().size() == 1);
    CHECK(stored->getExtensions()[0].getGrantedBy() == f.lecturer);
    CHECK(stored->getExtensions()[0].getReason() == "Medical certificate");
}

TEST_CASE("FR-09, BR-03: extension rules") {
    AssignmentFixture f;
    int a = f.project();
    Session lect = f.as(f.lecturer);

    CHECK_THROWS_WITH(f.service.grantExtension(lect, a, f.student, hoursAfter(96), " "),
                      "An extension needs a reason");
    CHECK_THROWS_WITH(f.service.grantExtension(lect, a, f.student, hoursAfter(48), "Sick"),
                      "The new deadline must be later than the student's current deadline");
    CHECK_THROWS_WITH(f.service.grantExtension(lect, a, f.otherStudent, hoursAfter(96), "Sick"),
                      "That student is not enrolled in this course");
    CHECK_THROWS_WITH(f.service.grantExtension(lect, 999, f.student, hoursAfter(96), "Sick"),
                      "Assignment not found");
    CHECK_THROWS_AS(f.service.grantExtension(f.as(f.student), a, f.student, hoursAfter(96), "Sick"), AccessDenied);

    // A second extension must go past the first one.
    f.service.grantExtension(lect, a, f.student, hoursAfter(96), "Sick");
    CHECK_THROWS(f.service.grantExtension(lect, a, f.student, hoursAfter(72), "Still sick"));
    f.service.grantExtension(lect, a, f.student, hoursAfter(120), "Still sick");
    CHECK(f.service.effectiveDeadline(a, f.student) == hoursAfter(120));

    // An extension after the deadline has passed must still end in the future.
    f.clock.set(hoursAfter(130));
    CHECK_THROWS_WITH(f.service.grantExtension(lect, a, f.student, hoursAfter(125), "Late request"),
                      "The new deadline must be in the future");
}

TEST_CASE("FR-10: a student sees the assignments of their own courses only") {
    AssignmentFixture f;
    f.project();
    f.courses.addLecturer(f.otherCourse, f.lecturer);
    f.service.createAssignment(f.as(f.lecturer), f.otherCourse, "Quiz", "", hoursAfter(24), 10);

    auto mine = f.service.listForStudent(f.as(f.student));
    REQUIRE(mine.size() == 1);
    CHECK(mine[0].getTitle() == "Project");
    CHECK_THROWS_AS(f.service.listForStudent(f.as(f.lecturer)), AccessDenied);
}

TEST_CASE("BR-05: who may list the assignments of a course") {
    AssignmentFixture f;
    f.project();
    CHECK(f.service.listForCourse(f.as(f.lecturer), f.course).size() == 1);
    CHECK(f.service.listForCourse(f.as(f.student), f.course).size() == 1);
    CHECK(f.service.listForCourse(f.as(f.hod), f.course).size() == 1);
    CHECK_THROWS_AS(f.service.listForCourse(f.as(f.otherStudent), f.course), AccessDenied);
    CHECK_THROWS_AS(f.service.listForCourse(f.as(f.lecturer), f.otherCourse), AccessDenied);

    int admin = f.addUser(Role::Administrator, "admin", "S001");
    CHECK_THROWS_AS(f.service.listForCourse(f.as(admin), f.course), AccessDenied);
    CHECK_THROWS_WITH(f.service.listForCourse(f.as(f.lecturer), 999), "Course not found");
}
