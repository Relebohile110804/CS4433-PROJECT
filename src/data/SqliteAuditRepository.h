#pragma once
#include "business/repositories/IAuditRepository.h"
#include "data/Database.h"

namespace lms::data {

class SqliteAuditRepository : public repositories::IAuditRepository {
public:
    explicit SqliteAuditRepository(Database& db) : db_(db) {}

    int append(const domain::AuditEntry& entry) override;
    std::vector<domain::AuditEntry> query(std::optional<int> departmentId, std::optional<int> courseId,
                                          std::optional<int> assignmentId) override;

private:
    Database& db_;
};

}  // namespace lms::data
