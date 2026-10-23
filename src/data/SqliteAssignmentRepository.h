#pragma once
#include "business/repositories/IAssignmentRepository.h"
#include "data/Database.h"

namespace lms::data {

class SqliteAssignmentRepository : public repositories::IAssignmentRepository {
public:
    explicit SqliteAssignmentRepository(Database& db) : db_(db) {}

    std::optional<domain::Assignment> findById(int id) override;
    std::vector<domain::Assignment> forCourse(int courseId) override;
    int add(const domain::Assignment& assignment) override;
    int addExtension(int assignmentId, const domain::Extension& extension) override;

private:
    std::vector<domain::Assignment> load(Statement& st);
    Database& db_;
};

}  // namespace lms::data
