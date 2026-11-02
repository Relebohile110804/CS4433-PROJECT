#include "business/services/ModerationService.h"

#include <algorithm>

#include "business/domain/Errors.h"

namespace lms::services {

using namespace lms::domain;

int ModerationService::ownDepartment(const Session& s) {
    s.require(Permission::ModerateMarks);
    std::optional<Department> d = courses_.departmentOfHod(s.getUser().getId());
    if (!d) throw AccessDenied("You are not the head of any department");
    return d->getId();
}

Assignment ModerationService::departmentAssignment(const Session& s, int assignmentId) {
    int dept = ownDepartment(s);
    std::optional<Assignment> a = assignments_.findById(assignmentId);
    if (!a) throw RuleViolation("Assignment not found");
    if (courses_.departmentOf(a->getCourseId()) != dept) throw AccessDenied("That assignment is not in your department");
    return *a;
}

Submission ModerationService::departmentSubmission(const Session& s, int submissionId) {
    std::optional<Submission> sub = submissions_.findById(submissionId);
    if (!sub) throw RuleViolation("Submission not found");
    departmentAssignment(s, sub->getAssignmentId());
    return *sub;
}

std::vector<Assignment> ModerationService::queue(const Session& s) {
    int dept = ownDepartment(s);
    std::vector<Assignment> result;
    for (const Course& c : courses_.coursesInDepartment(dept))
        for (Assignment& a : assignments_.forCourse(c.getId())) {
            std::vector<Submission> subs = submissions_.forAssignment(a.getId());
            // BR-06: any marked work, so released marks can still be corrected
            if (std::any_of(subs.begin(), subs.end(), [](const Submission& sub) { return sub.isMarked(); }))
                result.push_back(std::move(a));
        }
    return result;
}

std::vector<Submission> ModerationService::listForModeration(const Session& s, int assignmentId) {
    departmentAssignment(s, assignmentId);
    std::vector<Submission> result;
    for (Submission& sub : submissions_.forAssignment(assignmentId))
        if (sub.isMarked()) result.push_back(std::move(sub));
    return result;
}

void ModerationService::approve(const Session& s, int submissionId) {
    Submission sub = departmentSubmission(s, submissionId);
    sub.approve(s.getUser().getId(), clock_.now());
    submissions_.save(sub);
}

void ModerationService::adjust(const Session& s, int submissionId, double newMark, const std::string& reason) {
    Submission sub = departmentSubmission(s, submissionId);
    Assignment a = departmentAssignment(s, sub.getAssignmentId());
    if (isBlank(reason)) throw RuleViolation("A reason is needed to adjust a mark");
    if (newMark < 0 || newMark > a.getMaxMark())
        throw RuleViolation("The new mark must be from 0 to " + std::to_string(a.getMaxMark()));
    const std::optional<Mark>& current = sub.getMark();
    if (!current) throw RuleViolation("This submission has not been marked yet");

    double oldValue = current->getFinalMark();
    sub.adjust(newMark, s.getUser().getId(), clock_.now());
    auto t = tx_.begin();  // BR-07
    submissions_.save(sub);
    audit_.record(submissionId, s.getUser().getId(), oldValue, newMark, reason);
    t->commit();
}

void ModerationService::returnForRemarking(const Session& s, int submissionId, const std::string& comment) {
    Submission sub = departmentSubmission(s, submissionId);
    sub.returnToLecturer(comment);
    submissions_.save(sub);
}

int ModerationService::release(const Session& s, int assignmentId) {
    s.require(Permission::ReleaseMarks);
    departmentAssignment(s, assignmentId);
    std::vector<Submission> ready;
    for (Submission& sub : submissions_.forAssignment(assignmentId))
        if (sub.getStatus() == SubmissionStatus::Moderated) ready.push_back(std::move(sub));
    if (ready.empty()) throw RuleViolation("There are no moderated marks to release for this assignment");

    TimePoint now = clock_.now();
    auto t = tx_.begin();  // BR-06: all of the assignment's moderated marks are released together
    for (Submission& sub : ready) {
        sub.release(now);
        submissions_.save(sub);
    }
    t->commit();
    int count = static_cast<int>(ready.size());
    return count;
}

}  // namespace lms::services
