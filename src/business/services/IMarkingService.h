#pragma once
#include <string>
#include <vector>

#include "business/domain/Session.h"
#include "business/domain/Submission.h"

namespace lms::services {

// What the lecturer sees before saving a mark (UC-16 step 5).
struct MarkPreview {
    int rawMark;
    int daysLate;
    double penalty;
    double finalMark;
};

// UC-16 to UC-18, for lecturers.
class IMarkingService {
public:
    virtual ~IMarkingService() = default;
    // FR-14, FR-22: every submission for the assignment, with its status.
    virtual std::vector<domain::Submission> listSubmissions(const domain::Session& s, int assignmentId) = 0;
    virtual MarkPreview previewMark(const domain::Session& s, int submissionId, int rawMark) = 0;
    // FR-15, FR-19: saves the mark with the penalty applied and one audit entry, in one transaction.
    // The reason is needed when an existing mark is changed.
    virtual void mark(const domain::Session& s, int submissionId, int rawMark, const std::string& feedback,
                      const std::string& reason) = 0;
};

}  // namespace lms::services
