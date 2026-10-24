#include "doctest/doctest.h"

#include <filesystem>

#include "../TestHelpers.h"
#include "../fakes/FakeRepositories.h"
#include "../fakes/Fakes.h"
#include "business/domain/Errors.h"
#include "business/services/AdminService.h"
#include "business/services/AuthService.h"
#include "data/Database.h"
#include "data/SqliteCourseRepository.h"
#include "data/SqliteUserRepository.h"

using namespace lms::domain;
using lms::services::AdminService;
using lms::services::AuthService;

namespace {

// An administrator who is logged in, plus one HoD, one lecturer and one student.
struct AdminFixture {
    fakes::FakeUserRepository users;
    fakes::FakeCourseRepository courses;
    fakes::FakePasswordHasher hasher;
    fakes::FakeBackup backupFile;
    fakes::FakeClock clock{testutil::base()};
    AdminService admin{users, courses, hasher, backupFile};
    AuthService auth{users, hasher, clock};
    int adminId, hod, lecturer, student;

    AdminFixture() {
        adminId = admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
        Session s = asAdmin();
        hod = admin.createUser(s, Role::HeadOfDepartment, "hod", "Mrs Head", "S100", "password1");
        lecturer = admin.createUser(s, Role::Lecturer, "lect", "Mr Teacher", "S200", "password1");
        student = admin.createUser(s, Role::Student, "stu", "Ann Student", "2023001", "password1");
    }
    Session asAdmin() { return auth.login("admin", "admin-pass"); }
    Session asLecturer() { return auth.login("lect", "password1"); }
    int department() { return admin.createDepartment(asAdmin(), "Computer Science", hod); }
};

}  // namespace

TEST_CASE("first run creates one administrator and only once") {
    fakes::FakeUserRepository users;
    fakes::FakeCourseRepository courses;
    fakes::FakePasswordHasher hasher;
    fakes::FakeBackup backupFile;
    AdminService admin{users, courses, hasher, backupFile};

    CHECK(admin.needsFirstAdministrator());
    admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
    CHECK_FALSE(admin.needsFirstAdministrator());
    CHECK_THROWS_WITH(admin.createFirstAdministrator("admin2", "Other", "S002", "admin-pass"),
                      "An administrator already exists");
}

TEST_CASE("FR-04: the administrator creates users who can then log in") {
    AdminFixture f;
    Session s = f.auth.login("stu", "password1");
    CHECK(s.getUser().getRole() == Role::Student);
    CHECK(s.getUser().getPersonNumber() == "2023001");
    CHECK(f.users.findById(f.student)->getPasswordHash() == "hashed:password1");
}

TEST_CASE("FR-04: new users are checked before they are saved") {
    AdminFixture f;
    Session s = f.asAdmin();
    CHECK_THROWS_WITH(f.admin.createUser(s, Role::Student, "stu", "Other", "2023002", "password1"),
                      "The username stu is already taken");
    CHECK_THROWS_WITH(f.admin.createUser(s, Role::Student, "stu2", "Other", "2023001", "password1"),
                      "The number 2023001 is already used");
    CHECK_THROWS_WITH(f.admin.createUser(s, Role::Student, "stu2", "Other", "2023002", "short"),
                      "The password must be at least 8 characters");
    CHECK_THROWS_WITH(f.admin.createUser(s, Role::Student, " ", "Other", "2023002", "password1"),
                      "Username cannot be empty");
    CHECK_THROWS_WITH(f.admin.createUser(s, Role::Student, "stu2", "", "2023002", "password1"),
                      "Full name cannot be empty");
    CHECK(f.admin.listUsers(s, Role::Student).size() == 1);
}

TEST_CASE("BR-05: only the administrator can use the admin service") {
    AdminFixture f;
    Session lect = f.asLecturer();
    CHECK_THROWS_AS(f.admin.createUser(lect, Role::Student, "x", "X", "1", "password1"), AccessDenied);
    CHECK_THROWS_AS(f.admin.createDepartment(lect, "Maths", f.hod), AccessDenied);
    CHECK_THROWS_AS(f.admin.enrolStudent(lect, 1, f.student), AccessDenied);
    CHECK_THROWS_AS(f.admin.backup(lect, "backup.db"), AccessDenied);
}

TEST_CASE("FR-04: editing, resetting and deactivating users") {
    AdminFixture f;
    Session s = f.asAdmin();

    f.admin.updateUser(s, f.student, "Ann Mokoena");
    CHECK(f.users.findById(f.student)->getFullName() == "Ann Mokoena");

    // A password reset also ends a lockout.
    for (int i = 0; i < 5; ++i) CHECK_THROWS(f.auth.login("stu", "wrong-pass"));
    f.admin.resetPassword(s, f.student, "fresh-pass");
    CHECK_NOTHROW(f.auth.login("stu", "fresh-pass"));

    f.admin.deactivateUser(s, f.student);
    CHECK_THROWS_WITH(f.auth.login("stu", "fresh-pass"),
                      "This account has been deactivated. Please see the administrator");
    CHECK_THROWS_WITH(f.admin.deactivateUser(s, f.student), "This account is already deactivated");
    CHECK_THROWS_WITH(f.admin.deactivateUser(s, f.adminId), "You cannot deactivate your own account");
    CHECK_THROWS_WITH(f.admin.updateUser(s, 999, "Nobody"), "User not found");
}

TEST_CASE("FR-05: departments need a free, active Head of Department") {
    AdminFixture f;
    Session s = f.asAdmin();
    int dept = f.department();
    CHECK(f.admin.listDepartments(s).size() == 1);

    CHECK_THROWS_WITH(f.admin.createDepartment(s, "Maths", f.hod),
                      "This Head of Department already heads Computer Science");
    CHECK_THROWS_WITH(f.admin.createDepartment(s, "Maths", f.lecturer), "No Head of Department has that id");

    int hod2 = f.admin.createUser(s, Role::HeadOfDepartment, "hod2", "Mr Other", "S101", "password1");
    CHECK_THROWS_WITH(f.admin.createDepartment(s, "Computer Science", hod2),
                      "A department called Computer Science already exists");

    // Editing: rename and hand the department to a new HoD.
    f.admin.updateDepartment(s, dept, "Computing", hod2);
    auto d = f.courses.findDepartment(dept);
    CHECK(d->getName() == "Computing");
    CHECK(d->getHodId() == hod2);
}

TEST_CASE("FR-05: courses have a unique code inside an existing department") {
    AdminFixture f;
    Session s = f.asAdmin();
    int dept = f.department();
    int course = f.admin.createCourse(s, dept, "CS4433", "Software Engineering");

    CHECK_THROWS_WITH(f.admin.createCourse(s, dept, "CS4433", "Again"), "A course with code CS4433 already exists");
    CHECK_THROWS_WITH(f.admin.createCourse(s, 999, "CS1000", "Intro"), "Department not found");
    CHECK_THROWS_WITH(f.admin.createCourse(s, dept, "", "Intro"), "Course code cannot be empty");

    int other = f.admin.createCourse(s, dept, "CS1000", "Intro");
    CHECK_THROWS_WITH(f.admin.updateCourse(s, other, "CS4433", "Intro"), "A course with code CS4433 already exists");
    f.admin.updateCourse(s, course, "CS4433", "Software Engineering II");
    CHECK(f.courses.findCourse(course)->getTitle() == "Software Engineering II");
    CHECK(f.admin.listCourses(s).size() == 2);
}

TEST_CASE("FR-06 and FR-07: lecturers and students are linked to courses") {
    AdminFixture f;
    Session s = f.asAdmin();
    int course = f.admin.createCourse(s, f.department(), "CS4433", "Software Engineering");

    f.admin.assignLecturer(s, course, f.lecturer);
    CHECK(f.courses.isTeaching(f.lecturer, course));
    CHECK_THROWS_WITH(f.admin.assignLecturer(s, course, f.lecturer), "Mr Teacher already teaches CS4433");
    CHECK_THROWS_WITH(f.admin.assignLecturer(s, course, f.student), "No Lecturer has that id");
    f.admin.removeLecturer(s, course, f.lecturer);
    CHECK_FALSE(f.courses.isTeaching(f.lecturer, course));
    CHECK_THROWS_WITH(f.admin.removeLecturer(s, course, f.lecturer), "That lecturer does not teach CS4433");

    f.admin.enrolStudent(s, course, f.student);
    CHECK(f.courses.isEnrolled(f.student, course));
    CHECK_THROWS_WITH(f.admin.enrolStudent(s, course, f.student), "Ann Student is already enrolled in CS4433");
    CHECK_THROWS_WITH(f.admin.enrolStudent(s, course, f.lecturer), "No Student has that id");
    CHECK_THROWS_WITH(f.admin.enrolStudent(s, 999, f.student), "Course not found");
    f.admin.withdrawStudent(s, course, f.student);
    CHECK_FALSE(f.courses.isEnrolled(f.student, course));
    CHECK_THROWS_WITH(f.admin.withdrawStudent(s, course, f.student), "That student is not enrolled in CS4433");

    // A deactivated student cannot be enrolled.
    f.admin.deactivateUser(s, f.student);
    CHECK_THROWS_WITH(f.admin.enrolStudent(s, course, f.student), "Ann Student is deactivated");
}

TEST_CASE("FR-23: backup and restore pass the file name to the database") {
    AdminFixture f;
    Session s = f.asAdmin();
    f.admin.backup(s, "lms_backup.db");
    CHECK(f.backupFile.lastBackup == "lms_backup.db");
    f.admin.restore(s, "lms_backup.db");
    CHECK(f.backupFile.lastRestore == "lms_backup.db");
    CHECK_THROWS_WITH(f.admin.backup(s, " "), "Please give a file name for the backup");
}

TEST_CASE("FR-23: a real backup brings back data deleted after it was taken") {
    namespace fs = std::filesystem;
    fs::path file = fs::temp_directory_path() / "lms_admin_backup_test.db";
    fs::remove(file);

    lms::data::Database db(":memory:");
    lms::data::SqliteUserRepository users(db);
    lms::data::SqliteCourseRepository courses(db);
    fakes::FakePasswordHasher hasher;
    fakes::FakeClock clock{testutil::base()};
    AdminService admin(users, courses, hasher, db);
    AuthService auth(users, hasher, clock);

    admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
    Session s = auth.login("admin", "admin-pass");
    admin.createUser(s, Role::Student, "stu", "Ann Student", "2023001", "password1");
    admin.backup(s, file.string());

    admin.createUser(s, Role::Student, "late", "Added Later", "2023002", "password1");
    CHECK(admin.listUsers(s, Role::Student).size() == 2);
    admin.restore(s, file.string());
    CHECK(admin.listUsers(s, Role::Student).size() == 1);

    fs::remove(file);
}
