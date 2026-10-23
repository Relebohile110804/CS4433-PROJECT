#pragma once
#include <optional>
#include <vector>

#include "business/domain/AuditEntry.h"

namespace lms::repositories {

// Append-only: there is no update or delete operation (BR-07).
class IAuditRepository {
public:
    virtual ~IAuditRepository() = default;
    virtual int append(const domain::AuditEntry& entry) = 0;
    // Entries oldest first; each filter is ignored when empty (FR-20).
    virtual std::vector<domain::AuditEntry> query(std::optional<int> departmentId, std::optional<int> courseId,
                                                  std::optional<int> assignmentId) = 0;
};

}  // namespace lms::repositories
