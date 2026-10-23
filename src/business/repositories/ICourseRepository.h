#pragma once
#include <optional>
#include <string>
#include <vector>

#include "business/domain/Course.h"
#include "business/domain/Department.h"

namespace lms::repositories {

class ICourseRepository {
public:
    virtual ~ICourseRepository() = default;

    // Departments
    virtual int addDepartment(const std::string& name, int hodId) = 0;
    virtual void updateDepartment(const domain::Department& department) = 0;
    virtual std::optional<domain::Department> findDepartment(int id) = 0;
    virtual std::optional<domain::Department> departmentOfHod(int hodId) = 0;
    virtual std::vector<domain::Department> listDepartments() = 0;

    // Courses
    virtual int addCourse(int departmentId, const std::string& code, const std::string& title) = 0;
    virtual void updateCourse(const domain::Course& course) = 0;
    virtual std::optional<domain::Course> findCourse(int id) = 0;
    virtual std::optional<domain::Course> findCourseByCode(const std::string& code) = 0;
    virtual std::vector<domain::Course> listCourses() = 0;
    virtual std::vector<domain::Course> coursesInDepartment(int departmentId) = 0;
    virtual std::vector<domain::Course> coursesForStudent(int studentId) = 0;
    virtual std::vector<domain::Course> coursesForLecturer(int lecturerId) = 0;
    virtual int departmentOf(int courseId) = 0;

    // Lecturers and enrolments
    virtual void addLecturer(int courseId, int lecturerId) = 0;
    virtual void removeLecturer(int courseId, int lecturerId) = 0;
    virtual bool isTeaching(int lecturerId, int courseId) = 0;
    virtual void enrol(int courseId, int studentId) = 0;
    virtual void withdraw(int courseId, int studentId) = 0;
    virtual bool isEnrolled(int studentId, int courseId) = 0;
};

}  // namespace lms::repositories
