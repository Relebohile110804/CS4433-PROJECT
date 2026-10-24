#pragma once
// In-memory repositories for unit testing the services without SQLite (NFR-12, R12).
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "business/ports/IBackup.h"
#include "business/repositories/ICourseRepository.h"
#include "business/repositories/IUserRepository.h"
#include "business/services/UserFactory.h"

namespace fakes {

class FakeUserRepository : public lms::repositories::IUserRepository {
public:
    std::unique_ptr<lms::domain::User> findById(int id) override {
        auto it = rows_.find(id);
        return it == rows_.end() ? nullptr : copy(*it->second);
    }
    std::unique_ptr<lms::domain::User> findByUsername(const std::string& username) override {
        for (auto& [id, u] : rows_)
            if (u->getUsername() == username) return copy(*u);
        return nullptr;
    }
    bool personNumberExists(const std::string& number) override {
        for (auto& [id, u] : rows_)
            if (u->getPersonNumber() == number) return true;
        return false;
    }
    std::vector<std::unique_ptr<lms::domain::User>> list(std::optional<lms::domain::Role> role) override {
        std::vector<std::unique_ptr<lms::domain::User>> result;
        for (auto& [id, u] : rows_)
            if (!role || u->getRole() == *role) result.push_back(copy(*u));
        return result;
    }
    int add(const lms::domain::User& user) override {
        for (auto& [id, u] : rows_)
            if (u->getUsername() == user.getUsername()) throw std::runtime_error("UNIQUE constraint failed");
        int id = nextId_++;
        auto stored = copy(user);
        stored->setId(id);
        rows_[id] = std::move(stored);
        return id;
    }
    void update(const lms::domain::User& user) override { rows_.at(user.getId()) = copy(user); }

private:
    static std::unique_ptr<lms::domain::User> copy(const lms::domain::User& u) {
        auto c = lms::services::UserFactory::create(u.getRole(), u.getId(), u.getUsername(), u.getFullName(),
                                                    u.getPersonNumber());
        if (!u.getPasswordHash().empty()) c->setPasswordHash(u.getPasswordHash());
        c->restoreLoginState(u.isActive(), u.getFailedLogins(), u.getLockedUntil());
        return c;
    }
    std::map<int, std::unique_ptr<lms::domain::User>> rows_;
    int nextId_ = 1;
};

// Departments and courses in maps. Lecturers and students are kept in each Course's id sets.
class FakeCourseRepository : public lms::repositories::ICourseRepository {
public:
    int addDepartment(const std::string& name, int hodId) override {
        int id = nextId_++;
        departments_.emplace(id, lms::domain::Department(id, name, hodId));
        return id;
    }
    void updateDepartment(const lms::domain::Department& d) override { departments_.at(d.getId()) = d; }
    std::optional<lms::domain::Department> findDepartment(int id) override {
        auto it = departments_.find(id);
        if (it == departments_.end()) return std::nullopt;
        return it->second;
    }
    std::optional<lms::domain::Department> departmentOfHod(int hodId) override {
        for (auto& [id, d] : departments_)
            if (d.getHodId() == hodId) return d;
        return std::nullopt;
    }
    std::vector<lms::domain::Department> listDepartments() override {
        std::vector<lms::domain::Department> result;
        for (auto& [id, d] : departments_) result.push_back(d);
        return result;
    }

    int addCourse(int departmentId, const std::string& code, const std::string& title) override {
        int id = nextId_++;
        courses_.emplace(id, lms::domain::Course(id, departmentId, code, title));
        return id;
    }
    void updateCourse(const lms::domain::Course& c) override {
        auto& stored = courses_.at(c.getId());
        stored.rename(c.getCode(), c.getTitle());
    }
    std::optional<lms::domain::Course> findCourse(int id) override {
        auto it = courses_.find(id);
        if (it == courses_.end()) return std::nullopt;
        return it->second;
    }
    std::optional<lms::domain::Course> findCourseByCode(const std::string& code) override {
        for (auto& [id, c] : courses_)
            if (c.getCode() == code) return c;
        return std::nullopt;
    }
    std::vector<lms::domain::Course> listCourses() override { return where([](const lms::domain::Course&) { return true; }); }
    std::vector<lms::domain::Course> coursesInDepartment(int departmentId) override {
        return where([&](const lms::domain::Course& c) { return c.getDepartmentId() == departmentId; });
    }
    std::vector<lms::domain::Course> coursesForStudent(int studentId) override {
        return where([&](const lms::domain::Course& c) { return c.hasStudent(studentId); });
    }
    std::vector<lms::domain::Course> coursesForLecturer(int lecturerId) override {
        return where([&](const lms::domain::Course& c) { return c.isTaughtBy(lecturerId); });
    }
    int departmentOf(int courseId) override { return courses_.at(courseId).getDepartmentId(); }

    void addLecturer(int courseId, int lecturerId) override { courses_.at(courseId).addLecturer(lecturerId); }
    void removeLecturer(int courseId, int lecturerId) override { courses_.at(courseId).removeLecturer(lecturerId); }
    bool isTeaching(int lecturerId, int courseId) override {
        auto it = courses_.find(courseId);
        return it != courses_.end() && it->second.isTaughtBy(lecturerId);
    }
    void enrol(int courseId, int studentId) override { courses_.at(courseId).enrol(studentId); }
    void withdraw(int courseId, int studentId) override { courses_.at(courseId).withdraw(studentId); }
    bool isEnrolled(int studentId, int courseId) override {
        auto it = courses_.find(courseId);
        return it != courses_.end() && it->second.hasStudent(studentId);
    }

private:
    template <class Pred>
    std::vector<lms::domain::Course> where(Pred keep) {
        std::vector<lms::domain::Course> result;
        for (auto& [id, c] : courses_)
            if (keep(c)) result.push_back(c);
        return result;
    }
    std::map<int, lms::domain::Department> departments_;
    std::map<int, lms::domain::Course> courses_;
    int nextId_ = 1;
};

// Remembers the last backup and restore paths instead of copying files.
class FakeBackup : public lms::ports::IBackup {
public:
    void backupTo(const std::string& path) override { lastBackup = path; }
    void restoreFrom(const std::string& path) override { lastRestore = path; }
    std::string lastBackup;
    std::string lastRestore;
};

}  // namespace fakes
