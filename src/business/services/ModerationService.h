#pragma once
#include "business/ports/IClock.h"
#include "business/repositories/IAssignmentRepository.h"
#include "business/repositories/ICourseRepository.h"
#include "business/repositories/ISubmissionRepository.h"
#include "business/repositories/ITransactionManager.h"
#include "business/services/IAuditService.h"
#include "business/services/IModerationService.h"

namespace lms::services {

class ModerationService : public IModerationService {
public:
    ModerationService(repositories::ISubmissionRepository& submissions,
                      repositories::IAssignmentRepository& assignments, repositories::ICourseRepository& courses,
                      IAuditService& audit, ports::IClock& clock, repositories::ITransactionManager& tx)
        : submissions_(submissions), assignments_(assignments), courses_(courses), audit_(audit), clock_(clock), tx_(tx) {}

    std::vector<domain::Assignment> queue(const domain::Session& s) override;
    std::vector<domain::Submission> listForModeration(const domain::Session& s, int assignmentId) override;
    void approve(const domain::Session& s, int submissionId) override;
    void adjust(const domain::Session& s, int submissionId, double newMark, const std::string& reason) override;
    void returnForRemarking(const domain::Session& s, int submissionId, const std::string& comment) override;
    int release(const domain::Session& s, int assignmentId) override;

private:
    int ownDepartment(const domain::Session& s);
    domain::Assignment departmentAssignment(const domain::Session& s, int assignmentId);
    domain::Submission departmentSubmission(const domain::Session& s, int submissionId);

    repositories::ISubmissionRepository& submissions_;
    repositories::IAssignmentRepository& assignments_;
    repositories::ICourseRepository& courses_;
    IAuditService& audit_;
    ports::IClock& clock_;
    repositories::ITransactionManager& tx_;
};

}  // namespace lms::services
