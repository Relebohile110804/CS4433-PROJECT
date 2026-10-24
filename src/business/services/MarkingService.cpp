#include "business/services/MarkingService.h"

#include "business/domain/Errors.h"

namespace lms::services {

using namespace lms::domain;

// The assignment, after checking the lecturer teaches its course (BR-05).
Assignment MarkingService::taughtAssignment(const Session& s, int assignmentId) {
    s.require(Permission::MarkSubmission);
    std::optional<Assignment> a = assignments_.findById(assignmentId);
    if (!a) throw RuleViolation("Assignment not found");
    if (!courses_.isTeaching(s.getUser().getId(), a->getCourseId())) throw AccessDenied("You do not teach this course");
    return *a;
}

std::vector<Submission> MarkingService::listSubmissions(const Session& s, int assignmentId) {
    taughtAssignment(s, assignmentId);
    return submissions_.forAssignment(assignmentId);
}

// BR-01: days late are counted from the student's effective deadline at the time of marking,
// so an extension granted after a late submission is taken into account.
MarkPreview MarkingService::calculate(const Submission& sub, const Assignment& a, int rawMark) {
    if (rawMark < 0 || rawMark > a.getMaxMark())
        throw RuleViolation("The raw mark must be from 0 to " + std::to_string(a.getMaxMark()));
    int days = policy_.daysLate(a.effectiveDeadlineFor(sub.getStudentId()), sub.currentVersion().getSubmittedAt());
    double final = policy_.finalMark(rawMark, a.getMaxMark(), days);
    return MarkPreview{rawMark, days, rawMark - final, final};
}

MarkPreview MarkingService::previewMark(const Session& s, int submissionId, int rawMark) {
    std::optional<Submission> sub = submissions_.findById(submissionId);
    if (!sub) throw RuleViolation("Submission not found");
    Assignment a = taughtAssignment(s, sub->getAssignmentId());
    return calculate(*sub, a, rawMark);
}

void MarkingService::mark(const Session& s, int submissionId, int rawMark, const std::string& feedback,
                          const std::string& reason) {
    std::optional<Submission> sub = submissions_.findById(submissionId);
    if (!sub) throw RuleViolation("Submission not found");
    Assignment a = taughtAssignment(s, sub->getAssignmentId());
    MarkPreview p = calculate(*sub, a, rawMark);

    std::optional<double> oldValue;
    if (sub->getMark()) oldValue = sub->getMark()->getFinalMark();
    if (oldValue && isBlank(reason)) throw RuleViolation("A reason is needed to change a mark");

    sub->recordMark(Mark(rawMark, p.penalty, p.finalMark, feedback, s.getUser().getId(), clock_.now()));
    auto t = tx_.begin();  // BR-07: the mark and its audit entry are saved together or not at all
    submissions_.save(*sub);
    audit_.record(submissionId, s.getUser().getId(), oldValue, p.finalMark, oldValue ? reason : "First mark");
    t->commit();
}

}  // namespace lms::services
