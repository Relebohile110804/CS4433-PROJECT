#pragma once
#include "business/ports/IClock.h"
#include "business/ports/IFileStore.h"
#include "business/repositories/IAssignmentRepository.h"
#include "business/repositories/ICourseRepository.h"
#include "business/repositories/ISubmissionRepository.h"
#include "business/repositories/ITransactionManager.h"
#include "business/services/IPenaltyPolicy.h"
#include "business/services/ISubmissionService.h"

namespace lms::services {

class SubmissionService : public ISubmissionService {
public:
    static constexpr std::uintmax_t MaxFileSize = std::uintmax_t{10} * 1024 * 1024;  // UC-09: 10 MB

    SubmissionService(repositories::ISubmissionRepository& submissions, repositories::IAssignmentRepository& assignments,
                      repositories::ICourseRepository& courses, ports::IFileStore& files, const IPenaltyPolicy& policy,
                      ports::IClock& clock, repositories::ITransactionManager& tx)
        : submissions_(submissions),
          assignments_(assignments),
          courses_(courses),
          files_(files),
          policy_(policy),
          clock_(clock),
          tx_(tx) {}

    int previewDaysLate(const domain::Session& s, int assignmentId) override;
    void checkCanSubmit(const domain::Session& s, int assignmentId) override;
    void validateFile(const std::string& filePath) override;
    int penaltyPercent(int daysLate) const override;
    domain::SubmissionVersion submit(const domain::Session& s, int assignmentId, const std::string& filePath) override;
    std::optional<domain::Submission> findOwn(const domain::Session& s, int assignmentId) override;
    domain::Mark viewReleasedMark(const domain::Session& s, int assignmentId) override;

    // UC-09: .pdf, .docx, .txt, .zip, .cpp or .h, in any letter case.
    static bool isAllowedFileType(const std::string& filePath);

private:
    domain::Assignment ownAssignment(const domain::Session& s, int assignmentId);
    void checkFile(const std::string& filePath);

    repositories::ISubmissionRepository& submissions_;
    repositories::IAssignmentRepository& assignments_;
    repositories::ICourseRepository& courses_;
    ports::IFileStore& files_;
    const IPenaltyPolicy& policy_;
    ports::IClock& clock_;
    repositories::ITransactionManager& tx_;
};

}  // namespace lms::services
