#pragma once
#include "business/repositories/ISubmissionRepository.h"
#include "data/Database.h"

namespace lms::data {

class SqliteSubmissionRepository : public repositories::ISubmissionRepository {
public:
    explicit SqliteSubmissionRepository(Database& db) : db_(db) {}

    std::optional<domain::Submission> findById(int id) override;
    std::optional<domain::Submission> find(int assignmentId, int studentId) override;
    std::vector<domain::Submission> forAssignment(int assignmentId) override;
    std::vector<domain::Submission> forStudent(int studentId) override;
    int save(const domain::Submission& submission) override;

private:
    std::vector<domain::Submission> load(Statement& st);
    Database& db_;
};

}  // namespace lms::data
