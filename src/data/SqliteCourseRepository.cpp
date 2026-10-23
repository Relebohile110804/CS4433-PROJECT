#include "data/SqliteCourseRepository.h"

#include <chrono>

#include "data/TimeText.h"

namespace lms::data {

using namespace lms::domain;

// ---- Departments -------------------------------------------------------------

int SqliteCourseRepository::addDepartment(const std::string& name, int hodId) {
    Statement st = db_.prepare("INSERT INTO departments (name, hod_id) VALUES (?1, ?2)");
    st.bindText(1, name).bindInt(2, hodId).run();
    return static_cast<int>(db_.lastInsertId());
}

void SqliteCourseRepository::updateDepartment(const Department& department) {
    Statement st = db_.prepare("UPDATE departments SET name = ?2, hod_id = ?3 WHERE department_id = ?1");
    st.bindInt(1, department.getId()).bindText(2, department.getName()).bindInt(3, department.getHodId()).run();
}

std::vector<Department> SqliteCourseRepository::loadDepartments(Statement& st) {
    std::vector<Department> result;
    while (st.step()) result.emplace_back(st.columnInt(0), st.columnText(1), st.columnInt(2));
    return result;
}

std::optional<Department> SqliteCourseRepository::findDepartment(int id) {
    Statement st = db_.prepare("SELECT department_id, name, hod_id FROM departments WHERE department_id = ?1");
    st.bindInt(1, id);
    auto found = loadDepartments(st);
    if (found.empty()) return std::nullopt;
    return found.front();
}

std::optional<Department> SqliteCourseRepository::departmentOfHod(int hodId) {
    Statement st = db_.prepare("SELECT department_id, name, hod_id FROM departments WHERE hod_id = ?1");
    st.bindInt(1, hodId);
    auto found = loadDepartments(st);
    if (found.empty()) return std::nullopt;
    return found.front();
}

std::vector<Department> SqliteCourseRepository::listDepartments() {
    Statement st = db_.prepare("SELECT department_id, name, hod_id FROM departments ORDER BY name");
    return loadDepartments(st);
}

// ---- Courses -----------------------------------------------------------------

int SqliteCourseRepository::addCourse(int departmentId, const std::string& code, const std::string& title) {
    Statement st = db_.prepare("INSERT INTO courses (department_id, code, title) VALUES (?1, ?2, ?3)");
    st.bindInt(1, departmentId).bindText(2, code).bindText(3, title).run();
    return static_cast<int>(db_.lastInsertId());
}

void SqliteCourseRepository::updateCourse(const Course& course) {
    Statement st = db_.prepare("UPDATE courses SET department_id = ?2, code = ?3, title = ?4 WHERE course_id = ?1");
    st.bindInt(1, course.getId())
        .bindInt(2, course.getDepartmentId())
        .bindText(3, course.getCode())
        .bindText(4, course.getTitle())
        .run();
}

// Reads course rows (course_id, department_id, code, title), then fills in each
// course's lecturers and students from the junction tables.
std::vector<Course> SqliteCourseRepository::loadCourses(Statement& st) {
    std::vector<Course> courses;
    while (st.step()) courses.emplace_back(st.columnInt(0), st.columnInt(1), st.columnText(2), st.columnText(3));
    for (Course& c : courses) {
        Statement lect = db_.prepare("SELECT lecturer_id FROM course_lecturers WHERE course_id = ?1");
        lect.bindInt(1, c.getId());
        while (lect.step()) c.addLecturer(lect.columnInt(0));
        Statement stud = db_.prepare("SELECT student_id FROM enrolments WHERE course_id = ?1");
        stud.bindInt(1, c.getId());
        while (stud.step()) c.enrol(stud.columnInt(0));
    }
    return courses;
}

std::optional<Course> SqliteCourseRepository::findCourse(int id) {
    Statement st = db_.prepare("SELECT course_id, department_id, code, title FROM courses WHERE course_id = ?1");
    st.bindInt(1, id);
    auto found = loadCourses(st);
    if (found.empty()) return std::nullopt;
    return found.front();
}

std::optional<Course> SqliteCourseRepository::findCourseByCode(const std::string& code) {
    Statement st = db_.prepare("SELECT course_id, department_id, code, title FROM courses WHERE code = ?1");
    st.bindText(1, code);
    auto found = loadCourses(st);
    if (found.empty()) return std::nullopt;
    return found.front();
}

std::vector<Course> SqliteCourseRepository::listCourses() {
    Statement st = db_.prepare("SELECT course_id, department_id, code, title FROM courses ORDER BY code");
    return loadCourses(st);
}

std::vector<Course> SqliteCourseRepository::coursesInDepartment(int departmentId) {
    Statement st = db_.prepare(
        "SELECT course_id, department_id, code, title FROM courses WHERE department_id = ?1 ORDER BY code");
    st.bindInt(1, departmentId);
    return loadCourses(st);
}

std::vector<Course> SqliteCourseRepository::coursesForStudent(int studentId) {
    Statement st = db_.prepare(
        "SELECT c.course_id, c.department_id, c.code, c.title FROM courses c "
        "JOIN enrolments e ON e.course_id = c.course_id WHERE e.student_id = ?1 ORDER BY c.code");
    st.bindInt(1, studentId);
    return loadCourses(st);
}

std::vector<Course> SqliteCourseRepository::coursesForLecturer(int lecturerId) {
    Statement st = db_.prepare(
        "SELECT c.course_id, c.department_id, c.code, c.title FROM courses c "
        "JOIN course_lecturers l ON l.course_id = c.course_id WHERE l.lecturer_id = ?1 ORDER BY c.code");
    st.bindInt(1, lecturerId);
    return loadCourses(st);
}

int SqliteCourseRepository::departmentOf(int courseId) {
    Statement st = db_.prepare("SELECT department_id FROM courses WHERE course_id = ?1");
    st.bindInt(1, courseId);
    if (!st.step()) throw DatabaseError("No course with id " + std::to_string(courseId));
    return st.columnInt(0);
}

// ---- Lecturers and enrolments ------------------------------------------------

void SqliteCourseRepository::addLecturer(int courseId, int lecturerId) {
    Statement st = db_.prepare("INSERT OR IGNORE INTO course_lecturers (course_id, lecturer_id) VALUES (?1, ?2)");
    st.bindInt(1, courseId).bindInt(2, lecturerId).run();
}

void SqliteCourseRepository::removeLecturer(int courseId, int lecturerId) {
    Statement st = db_.prepare("DELETE FROM course_lecturers WHERE course_id = ?1 AND lecturer_id = ?2");
    st.bindInt(1, courseId).bindInt(2, lecturerId).run();
}

bool SqliteCourseRepository::isTeaching(int lecturerId, int courseId) {
    Statement st = db_.prepare("SELECT 1 FROM course_lecturers WHERE course_id = ?1 AND lecturer_id = ?2");
    st.bindInt(1, courseId).bindInt(2, lecturerId);
    return st.step();
}

void SqliteCourseRepository::enrol(int courseId, int studentId) {
    Statement st = db_.prepare(
        "INSERT OR IGNORE INTO enrolments (course_id, student_id, enrolled_at) VALUES (?1, ?2, ?3)");
    st.bindInt(1, courseId).bindInt(2, studentId).bindText(3, toIsoText(std::chrono::system_clock::now())).run();
}

void SqliteCourseRepository::withdraw(int courseId, int studentId) {
    Statement st = db_.prepare("DELETE FROM enrolments WHERE course_id = ?1 AND student_id = ?2");
    st.bindInt(1, courseId).bindInt(2, studentId).run();
}

bool SqliteCourseRepository::isEnrolled(int studentId, int courseId) {
    Statement st = db_.prepare("SELECT 1 FROM enrolments WHERE course_id = ?1 AND student_id = ?2");
    st.bindInt(1, courseId).bindInt(2, studentId);
    return st.step();
}

}  // namespace lms::data
