#pragma once
#include <set>
#include <string>

namespace lms::domain {

// A course in one department. The many-to-many "teaches" and "enrolled in"
// associations are kept as sets of user ids (course_lecturers and enrolments tables).
class Course {
public:
    Course(int id, int departmentId, std::string code, std::string title);

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    int getDepartmentId() const { return departmentId_; }
    const std::string& getCode() const { return code_; }
    const std::string& getTitle() const { return title_; }
    void rename(const std::string& code, const std::string& title);

    const std::set<int>& getLecturerIds() const { return lecturerIds_; }
    const std::set<int>& getStudentIds() const { return studentIds_; }
    bool isTaughtBy(int lecturerId) const { return lecturerIds_.count(lecturerId) > 0; }
    bool hasStudent(int studentId) const { return studentIds_.count(studentId) > 0; }
    void addLecturer(int lecturerId) { lecturerIds_.insert(lecturerId); }
    void removeLecturer(int lecturerId) { lecturerIds_.erase(lecturerId); }
    void enrol(int studentId) { studentIds_.insert(studentId); }
    void withdraw(int studentId) { studentIds_.erase(studentId); }

private:
    int id_;
    int departmentId_;
    std::string code_;
    std::string title_;
    std::set<int> lecturerIds_;
    std::set<int> studentIds_;
};

}  // namespace lms::domain
