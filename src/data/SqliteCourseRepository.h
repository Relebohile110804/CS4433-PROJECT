#pragma once
#include "business/repositories/ICourseRepository.h"
#include "data/Database.h"

namespace lms::data {

class SqliteCourseRepository : public repositories::ICourseRepository {
public:
    explicit SqliteCourseRepository(Database& db) : db_(db) {}

    int addDepartment(const std::string& name, int hodId) override;
    void updateDepartment(const domain::Department& department) override;
    std::optional<domain::Department> findDepartment(int id) override;
    std::optional<domain::Department> departmentOfHod(int hodId) override;
    std::vector<domain::Department> listDepartments() override;

    int addCourse(int departmentId, const std::string& code, const std::string& title) override;
    void updateCourse(const domain::Course& course) override;
    std::optional<domain::Course> findCourse(int id) override;
    std::optional<domain::Course> findCourseByCode(const std::string& code) override;
    std::vector<domain::Course> listCourses() override;
    std::vector<domain::Course> coursesInDepartment(int departmentId) override;
    std::vector<domain::Course> coursesForStudent(int studentId) override;
    std::vector<domain::Course> coursesForLecturer(int lecturerId) override;
    int departmentOf(int courseId) override;

    void addLecturer(int courseId, int lecturerId) override;
    void removeLecturer(int courseId, int lecturerId) override;
    bool isTeaching(int lecturerId, int courseId) override;
    void enrol(int courseId, int studentId) override;
    void withdraw(int courseId, int studentId) override;
    bool isEnrolled(int studentId, int courseId) override;

private:
    std::vector<domain::Course> loadCourses(Statement& st);
    std::vector<domain::Department> loadDepartments(Statement& st);
    Database& db_;
};

}  // namespace lms::data
