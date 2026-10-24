#pragma once
#include <optional>
#include <string>

#include "business/domain/Session.h"
#include "business/domain/Submission.h"

namespace lms::services {

// UC-09 to UC-13, for students.
class ISubmissionService {
public:
    virtual ~ISubmissionService() = default;
    // FR-13: how many days late a submission made now would be (shown before the student confirms).
    virtual int previewDaysLate(const domain::Session& s, int assignmentId) = 0;
    // FR-11, FR-12: stores the file and records a new version (Figure 7).
    virtual domain::SubmissionVersion submit(const domain::Session& s, int assignmentId,
                                             const std::string& filePath) = 0;
    // The student's own submission. The mark is left out until it is released (BR-05).
    virtual std::optional<domain::Submission> findOwn(const domain::Session& s, int assignmentId) = 0;
    // FR-18: raw mark, penalty, final mark and feedback, only after release.
    virtual domain::Mark viewReleasedMark(const domain::Session& s, int assignmentId) = 0;
};

}  // namespace lms::services
