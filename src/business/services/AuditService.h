#pragma once
#include "business/ports/IClock.h"
#include "business/repositories/IAuditRepository.h"
#include "business/repositories/ICourseRepository.h"
#include "business/services/IAuditService.h"

namespace lms::services {

class AuditService : public IAuditService {
public:
    AuditService(repositories::IAuditRepository& entries, repositories::ICourseRepository& courses, ports::IClock& clock)
        : entries_(entries), courses_(courses), clock_(clock) {}

    void record(int submissionId, int changedBy, std::optional<double> oldValue, double newValue,
                const std::string& reason) override;
    std::vector<domain::AuditEntry> viewLog(const domain::Session& s, std::optional<int> courseId,
                                            std::optional<int> assignmentId) override;

private:
    repositories::IAuditRepository& entries_;
    repositories::ICourseRepository& courses_;
    ports::IClock& clock_;
};

}  // namespace lms::services
