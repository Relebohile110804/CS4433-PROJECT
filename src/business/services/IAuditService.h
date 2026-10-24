#pragma once
#include <optional>
#include <string>
#include <vector>

#include "business/domain/AuditEntry.h"
#include "business/domain/Session.h"

namespace lms::services {

// UC-19 and UC-22.
class IAuditService {
public:
    virtual ~IAuditService() = default;
    // FR-19: called by MarkingService and ModerationService inside their transaction (BR-07).
    virtual void record(int submissionId, int changedBy, std::optional<double> oldValue, double newValue,
                        const std::string& reason) = 0;
    // FR-20: the administrator sees the whole school, an HoD only their own department.
    virtual std::vector<domain::AuditEntry> viewLog(const domain::Session& s, std::optional<int> courseId,
                                                    std::optional<int> assignmentId) = 0;
};

}  // namespace lms::services
