#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "business/domain/Course.h"
#include "business/domain/Department.h"
#include "business/domain/Session.h"

namespace lms::services {

// UC-03 to UC-07. Every operation needs an Administrator session (BR-05).
// updateUser, updateDepartment, updateCourse and removeLecturer fill the gaps found in Section 5.3.
class IAdminService {
public:
    virtual ~IAdminService() = default;

    // First run: when the database has no administrator yet, the program creates one
    // so that somebody can log in and set up the rest.
    virtual bool needsFirstAdministrator() = 0;
    virtual int createFirstAdministrator(const std::string& username, const std::string& fullName,
                                         const std::string& number, const std::string& password) = 0;

    // Users (FR-04)
    virtual int createUser(const domain::Session& s, domain::Role role, const std::string& username,
                           const std::string& fullName, const std::string& number, const std::string& password) = 0;
    virtual void updateUser(const domain::Session& s, int userId, const std::string& fullName) = 0;
    virtual void resetPassword(const domain::Session& s, int userId, const std::string& newPassword) = 0;
    virtual void deactivateUser(const domain::Session& s, int userId) = 0;
    virtual std::vector<std::unique_ptr<domain::User>> listUsers(const domain::Session& s,
                                                                 std::optional<domain::Role> role) = 0;

    // Departments and courses (FR-05)
    virtual int createDepartment(const domain::Session& s, const std::string& name, int hodId) = 0;
    virtual void updateDepartment(const domain::Session& s, int departmentId, const std::string& name, int hodId) = 0;
    virtual std::vector<domain::Department> listDepartments(const domain::Session& s) = 0;
    virtual int createCourse(const domain::Session& s, int departmentId, const std::string& code,
                             const std::string& title) = 0;
    virtual void updateCourse(const domain::Session& s, int courseId, const std::string& code,
                              const std::string& title) = 0;
    virtual std::vector<domain::Course> listCourses(const domain::Session& s) = 0;

    // Lecturers (FR-06) and enrolment (FR-07)
    virtual void assignLecturer(const domain::Session& s, int courseId, int lecturerId) = 0;
    virtual void removeLecturer(const domain::Session& s, int courseId, int lecturerId) = 0;
    virtual void enrolStudent(const domain::Session& s, int courseId, int studentId) = 0;
    virtual void withdrawStudent(const domain::Session& s, int courseId, int studentId) = 0;

    // Backup and restore (FR-23)
    virtual void backup(const domain::Session& s, const std::string& path) = 0;
    virtual void restore(const domain::Session& s, const std::string& path) = 0;
};

}  // namespace lms::services
