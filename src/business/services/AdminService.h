#pragma once
#include "business/ports/IBackup.h"
#include "business/ports/IPasswordHasher.h"
#include "business/repositories/ICourseRepository.h"
#include "business/repositories/IUserRepository.h"
#include "business/services/IAdminService.h"

namespace lms::services {

class AdminService : public IAdminService {
public:
    AdminService(repositories::IUserRepository& users, repositories::ICourseRepository& courses,
                 ports::IPasswordHasher& hasher, ports::IBackup& backup)
        : users_(users), courses_(courses), hasher_(hasher), backup_(backup) {}

    // First run: when the database has no administrator yet, main() creates one
    // so that somebody can log in and set up the rest.
    bool needsFirstAdministrator();
    int createFirstAdministrator(const std::string& username, const std::string& fullName,
                                 const std::string& number, const std::string& password);

    int createUser(const domain::Session& s, domain::Role role, const std::string& username,
                   const std::string& fullName, const std::string& number, const std::string& password) override;
    void updateUser(const domain::Session& s, int userId, const std::string& fullName) override;
    void resetPassword(const domain::Session& s, int userId, const std::string& newPassword) override;
    void deactivateUser(const domain::Session& s, int userId) override;
    std::vector<std::unique_ptr<domain::User>> listUsers(const domain::Session& s,
                                                         std::optional<domain::Role> role) override;

    int createDepartment(const domain::Session& s, const std::string& name, int hodId) override;
    void updateDepartment(const domain::Session& s, int departmentId, const std::string& name, int hodId) override;
    std::vector<domain::Department> listDepartments(const domain::Session& s) override;
    int createCourse(const domain::Session& s, int departmentId, const std::string& code,
                     const std::string& title) override;
    void updateCourse(const domain::Session& s, int courseId, const std::string& code,
                      const std::string& title) override;
    std::vector<domain::Course> listCourses(const domain::Session& s) override;

    void assignLecturer(const domain::Session& s, int courseId, int lecturerId) override;
    void removeLecturer(const domain::Session& s, int courseId, int lecturerId) override;
    void enrolStudent(const domain::Session& s, int courseId, int studentId) override;
    void withdrawStudent(const domain::Session& s, int courseId, int studentId) override;

    void backup(const domain::Session& s, const std::string& path) override;
    void restore(const domain::Session& s, const std::string& path) override;

private:
    int addUser(domain::Role role, const std::string& username, const std::string& fullName,
                const std::string& number, const std::string& password);
    std::unique_ptr<domain::User> activeUserWithRole(int userId, domain::Role role);
    domain::Course existingCourse(int courseId);
    void checkHodIsFree(int hodId, int departmentId);

    repositories::IUserRepository& users_;
    repositories::ICourseRepository& courses_;
    ports::IPasswordHasher& hasher_;
    ports::IBackup& backup_;
};

}  // namespace lms::services
