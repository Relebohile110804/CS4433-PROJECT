#pragma once
#include <string>
#include <vector>

#include "business/domain/Assignment.h"
#include "business/domain/Session.h"
#include "business/domain/Submission.h"

namespace lms::services {

// UC-20 and UC-21, for the Head of Department. Everything is limited to their own department (BR-05).
class IModerationService {
public:
    virtual ~IModerationService() = default;
    // Assignments in the department with at least one marked submission (waiting, returned,
    // moderated or released), so that a released mark can still be corrected (BR-06).
    virtual std::vector<domain::Assignment> queue(const domain::Session& s) = 0;
    // FR-16: every marked submission of the assignment.
    virtual std::vector<domain::Submission> listForModeration(const domain::Session& s, int assignmentId) = 0;
    virtual void approve(const domain::Session& s, int submissionId) = 0;
    // FR-16, BR-06: before or after release, with a reason; audited (FR-19).
    virtual void adjust(const domain::Session& s, int submissionId, double newMark, const std::string& reason) = 0;
    // FR-21
    virtual void returnForRemarking(const domain::Session& s, int submissionId, const std::string& comment) = 0;
    // FR-17: releases every Moderated submission of the assignment; returns how many.
    virtual int release(const domain::Session& s, int assignmentId) = 0;
};

}  // namespace lms::services
