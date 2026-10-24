#pragma once
// A small school for the submission, marking, moderation and audit tests:
// one department (HoD "hod") with course CS4433 taught by "lect", two enrolled
// students, and assignment "Project" (max 100) due 48 hours after base().
// A second department has its own HoD, to test that HoDs only see their own.
#include "../TestHelpers.h"
#include "../fakes/FakeRepositories.h"
#include "../fakes/Fakes.h"
#include "business/services/AuditService.h"
#include "business/services/MarkingService.h"
#include "business/services/ModerationService.h"
#include "business/services/SubmissionService.h"
#include "business/services/TenPercentPerDayPolicy.h"

struct ServiceFixture {
    fakes::FakeUserRepository users;
    fakes::FakeCourseRepository courses;
    fakes::FakeAssignmentRepository assignments;
    fakes::FakeSubmissionRepository submissions;
    fakes::FakeAuditRepository auditLog;
    fakes::FakeFileStore files;
    fakes::FakeTransactionManager tx;
    fakes::FakeClock clock{testutil::base()};
    lms::services::TenPercentPerDayPolicy policy;
    lms::services::AuditService audit{auditLog, courses, clock};
    lms::services::SubmissionService submitting{submissions, assignments, courses, files, policy, clock, tx};
    lms::services::MarkingService marking{submissions, assignments, courses, policy, audit, clock, tx};
    lms::services::ModerationService moderation{submissions, assignments, courses, audit, clock, tx};

    int admin, hod, otherHod, lecturer, student, student2, dept, otherDept, course, otherCourse, assignment;

    ServiceFixture() {
        using lms::domain::Role;
        admin = addUser(Role::Administrator, "admin", "S001");
        hod = addUser(Role::HeadOfDepartment, "hod", "S100");
        otherHod = addUser(Role::HeadOfDepartment, "hod2", "S101");
        lecturer = addUser(Role::Lecturer, "lect", "S200");
        student = addUser(Role::Student, "stu", "2023001");
        student2 = addUser(Role::Student, "stu2", "2023002");
        dept = courses.addDepartment("Computer Science", hod);
        otherDept = courses.addDepartment("Mathematics", otherHod);
        course = courses.addCourse(dept, "CS4433", "Software Engineering");
        otherCourse = courses.addCourse(otherDept, "MA1000", "Calculus");
        courses.addLecturer(course, lecturer);
        courses.enrol(course, student);
        courses.enrol(course, student2);
        assignment = assignments.add(
            lms::domain::Assignment(0, course, "Project", "Build the LMS", testutil::hoursAfter(48), 100));
        files.addSource("work.pdf", 2000);
    }

    int addUser(lms::domain::Role role, const std::string& username, const std::string& number) {
        auto u = lms::services::UserFactory::create(role, 0, username, username, number);
        u->setPasswordHash("x");
        return users.add(*u);
    }
    lms::domain::Session as(int userId) {
        return lms::domain::Session(std::shared_ptr<const lms::domain::User>(users.findById(userId)), clock.now());
    }
    // Submits work.pdf for the student at the given hour and returns the submission id.
    int submitAt(double hour, int who = 0) {
        int s = who ? who : student;
        clock.set(testutil::hoursAfter(hour));
        submitting.submit(as(s), assignment, "work.pdf");
        return submissions.find(assignment, s)->getId();
    }
    // A submission made on time and marked by the lecturer.
    int markedSubmission(int rawMark = 70, int who = 0) {
        int id = submitAt(10, who);
        clock.set(testutil::hoursAfter(60));
        marking.mark(as(lecturer), id, rawMark, "Good work", "");
        return id;
    }
};
