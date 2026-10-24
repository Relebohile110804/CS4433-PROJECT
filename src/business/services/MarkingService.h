#pragma once
#include "business/ports/IClock.h"
#include "business/repositories/IAssignmentRepository.h"
#include "business/repositories/ICourseRepository.h"
#include "business/repositories/ISubmissionRepository.h"
#include "business/repositories/ITransactionManager.h"
#include "business/services/IAuditService.h"
#include "business/services/IMarkingService.h"
#include "business/services/IPenaltyPolicy.h"

namespace lms::services {

class MarkingService : public IMarkingService {
public:
    MarkingService(repositories::ISubmissionRepository& submissions, repositories::IAssignmentRepository& assignments,
                   repositories::ICourseRepository& courses, const IPenaltyPolicy& policy, IAuditService& audit,
                   ports::IClock& clock, repositories::ITransactionManager& tx)
        : submissions_(submissions),
          assignments_(assignments),
          courses_(courses),
          policy_(policy),
          audit_(audit),
          clock_(clock),
          tx_(tx) {}

    std::vector<domain::Submission> listSubmissions(const domain::Session& s, int assignmentId) override;
    MarkPreview previewMark(const domain::Session& s, int submissionId, int rawMark) override;
    void mark(const domain::Session& s, int submissionId, int rawMark, const std::string& feedback,
              const std::string& reason) override;

private:
    domain::Assignment taughtAssignment(const domain::Session& s, int assignmentId);
    MarkPreview calculate(const domain::Submission& sub, const domain::Assignment& a, int rawMark);

    repositories::ISubmissionRepository& submissions_;
    repositories::IAssignmentRepository& assignments_;
    repositories::ICourseRepository& courses_;
    const IPenaltyPolicy& policy_;
    IAuditService& audit_;
    ports::IClock& clock_;
    repositories::ITransactionManager& tx_;
};

}  // namespace lms::services
