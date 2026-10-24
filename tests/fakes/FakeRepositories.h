#pragma once
// In-memory repositories for unit testing the services without SQLite (NFR-12, R12).
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "business/ports/IBackup.h"
#include "business/repositories/IAssignmentRepository.h"
#include "business/repositories/IAuditRepository.h"
#include "business/repositories/ICourseRepository.h"
#include "business/repositories/ISubmissionRepository.h"
#include "business/repositories/ITransactionManager.h"
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

// Assignments in a map; each one keeps its extensions, like the real repository returns them.
class FakeAssignmentRepository : public lms::repositories::IAssignmentRepository {
public:
    std::optional<lms::domain::Assignment> findById(int id) override {
        auto it = rows_.find(id);
        if (it == rows_.end()) return std::nullopt;
        return it->second;
    }
    std::vector<lms::domain::Assignment> forCourse(int courseId) override {
        std::vector<lms::domain::Assignment> result;
        for (auto& [id, a] : rows_)
            if (a.getCourseId() == courseId) result.push_back(a);
        return result;
    }
    int add(const lms::domain::Assignment& a) override {
        int id = nextId_++;
        lms::domain::Assignment stored = a;
        stored.setId(id);
        rows_.emplace(id, stored);
        return id;
    }
    int addExtension(int assignmentId, const lms::domain::Extension& e) override {
        lms::domain::Extension stored = e;
        stored.setId(nextId_++);
        rows_.at(assignmentId).restoreExtension(stored);
        return stored.getId();
    }

private:
    std::map<int, lms::domain::Assignment> rows_;
    int nextId_ = 1;
};

// Submissions in a map. failNextSave makes the next save throw, to test that nothing is half saved.
class FakeSubmissionRepository : public lms::repositories::ISubmissionRepository {
public:
    std::optional<lms::domain::Submission> findById(int id) override {
        auto it = rows_.find(id);
        if (it == rows_.end()) return std::nullopt;
        return it->second;
    }
    std::optional<lms::domain::Submission> find(int assignmentId, int studentId) override {
        for (auto& [id, sub] : rows_)
            if (sub.getAssignmentId() == assignmentId && sub.getStudentId() == studentId) return sub;
        return std::nullopt;
    }
    std::vector<lms::domain::Submission> forAssignment(int assignmentId) override {
        std::vector<lms::domain::Submission> result;
        for (auto& [id, sub] : rows_)
            if (sub.getAssignmentId() == assignmentId) result.push_back(sub);
        return result;
    }
    std::vector<lms::domain::Submission> forStudent(int studentId) override {
        std::vector<lms::domain::Submission> result;
        for (auto& [id, sub] : rows_)
            if (sub.getStudentId() == studentId) result.push_back(sub);
        return result;
    }
    int save(const lms::domain::Submission& sub) override {
        if (failNextSave) {
            failNextSave = false;
            throw std::runtime_error("disk full");
        }
        lms::domain::Submission stored = sub;
        if (stored.getId() == 0) stored.setId(nextId_++);
        rows_.insert_or_assign(stored.getId(), stored);
        return stored.getId();
    }
    bool failNextSave = false;

private:
    std::map<int, lms::domain::Submission> rows_;
    int nextId_ = 1;
};

// Keeps every entry and the filters of the last query. The filtering itself is
// done by SQL in the real repository, so here query() returns all entries.
class FakeAuditRepository : public lms::repositories::IAuditRepository {
public:
    int append(const lms::domain::AuditEntry& e) override {
        entries.push_back(e);
        return static_cast<int>(entries.size());
    }
    std::vector<lms::domain::AuditEntry> query(std::optional<int> departmentId, std::optional<int> courseId,
                                               std::optional<int> assignmentId) override {
        lastDepartment = departmentId;
        lastCourse = courseId;
        lastAssignment = assignmentId;
        return entries;
    }
    std::vector<lms::domain::AuditEntry> entries;
    std::optional<int> lastDepartment, lastCourse, lastAssignment;
};

// Counts transactions so tests can check that work was committed or rolled back.
class FakeTransactionManager : public lms::repositories::ITransactionManager {
public:
    class Tx : public lms::repositories::ITransaction {
    public:
        explicit Tx(FakeTransactionManager& m) : m_(m) {}
        ~Tx() override {
            if (!done_) ++m_.rolledBack;
        }
        void commit() override {
            done_ = true;
            ++m_.committed;
        }

    private:
        FakeTransactionManager& m_;
        bool done_ = false;
    };
    std::unique_ptr<lms::repositories::ITransaction> begin() override { return std::make_unique<Tx>(*this); }
    int committed = 0;
    int rolledBack = 0;
};

}  // namespace fakes
