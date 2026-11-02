#include "business/services/SubmissionService.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "business/domain/Errors.h"

namespace lms::services {

using namespace lms::domain;

bool SubmissionService::isAllowedFileType(const std::string& filePath) {
    std::size_t dot = filePath.find_last_of('.');
    std::size_t slash = filePath.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return false;
    std::string ext = filePath.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    static const char* const allowed[] = {".pdf", ".docx", ".txt", ".zip", ".cpp", ".h"};
    return std::any_of(std::begin(allowed), std::end(allowed), [&](const char* a) { return ext == a; });
}

// The assignment, after checking the student is enrolled in its course (BR-05).
Assignment SubmissionService::ownAssignment(const Session& s, int assignmentId) {
    s.require(Permission::SubmitWork);
    std::optional<Assignment> a = assignments_.findById(assignmentId);
    if (!a) throw RuleViolation("Assignment not found");
    if (!courses_.isEnrolled(s.getUser().getId(), a->getCourseId()))
        throw AccessDenied("You are not enrolled in this course");
    return *a;
}

void SubmissionService::checkFile(const std::string& filePath) {
    if (!files_.isReadableFile(filePath)) throw RuleViolation("The file cannot be found or opened: " + filePath);
    if (!isAllowedFileType(filePath))
        throw RuleViolation("Only .pdf, .docx, .txt, .zip, .cpp and .h files can be submitted");
    if (files_.fileSize(filePath) > MaxFileSize) throw RuleViolation("The file is larger than 10 MB");
}

int SubmissionService::previewDaysLate(const Session& s, int assignmentId) {
    Assignment a = ownAssignment(s, assignmentId);
    return policy_.daysLate(a.effectiveDeadlineFor(s.getUser().getId()), clock_.now());
}

void SubmissionService::checkCanSubmit(const Session& s, int assignmentId) {
    Assignment a = ownAssignment(s, assignmentId);
    std::optional<Submission> existing = submissions_.find(assignmentId, s.getUser().getId());
    if (!existing) return;  // BR-02: a first submission is always accepted, even late
    if (existing->getStatus() == SubmissionStatus::Late)
        throw RuleViolation("A late submission cannot be replaced (BR-02)");
    if (existing->getStatus() != SubmissionStatus::Submitted)
        throw RuleViolation("This work has already been marked, so it cannot be submitted again");
    if (policy_.daysLate(a.effectiveDeadlineFor(s.getUser().getId()), clock_.now()) > 0)
        throw RuleViolation("The deadline has passed, so the version already submitted stands (BR-02)");
}

void SubmissionService::validateFile(const std::string& filePath) { checkFile(filePath); }

int SubmissionService::penaltyPercent(int daysLate) const {
    // Worked out from the policy itself, so the message always matches BR-01.
    return static_cast<int>(std::lround(100 - policy_.finalMark(100, 100, daysLate)));
}

SubmissionVersion SubmissionService::submit(const Session& s, int assignmentId, const std::string& filePath) {
    Assignment a = ownAssignment(s, assignmentId);
    int studentId = s.getUser().getId();
    checkFile(filePath);

    TimePoint now = clock_.now();
    int daysLate = policy_.daysLate(a.effectiveDeadlineFor(studentId), now);
    std::optional<Submission> existing = submissions_.find(assignmentId, studentId);
    int versionNo = existing ? existing->currentVersion().getVersionNo() + 1 : 1;

    std::string key = "a" + std::to_string(assignmentId) + "_s" + std::to_string(studentId) + "_v" +
                      std::to_string(versionNo);
    std::string stored = files_.store(filePath, key);
    try {
        SubmissionVersion version(versionNo, stored, files_.sha256(stored), now);
        auto t = tx_.begin();
        if (existing) {
            existing->addVersion(version, daysLate);  // BR-02, BR-04
            submissions_.save(*existing);
        } else {
            submissions_.save(Submission::createNew(assignmentId, studentId, version, daysLate));
        }
        t->commit();
        return version;
    } catch (...) {
        // UC-09 E4: nothing was saved, so do not keep the copied file either.
        files_.remove(stored);
        throw;
    }
}

std::optional<Submission> SubmissionService::findOwn(const Session& s, int assignmentId) {
    ownAssignment(s, assignmentId);
    std::optional<Submission> sub = submissions_.find(assignmentId, s.getUser().getId());
    if (!sub || sub->isVisibleToStudent()) return sub;
    // Not released yet: give the student everything except the mark.
    return Submission(sub->getId(), sub->getAssignmentId(), sub->getStudentId(), sub->getStatus(),
                      sub->getDaysLate(), sub->getVersions(), std::nullopt, std::nullopt, std::nullopt,
                      std::nullopt, "");
}

Mark SubmissionService::viewReleasedMark(const Session& s, int assignmentId) {
    s.require(Permission::ViewOwnMarks);
    ownAssignment(s, assignmentId);
    std::optional<Submission> sub = submissions_.find(assignmentId, s.getUser().getId());
    if (!sub) throw RuleViolation("You have not submitted this assignment");
    const std::optional<Mark>& mark = sub->getMark();
    if (!sub->isVisibleToStudent() || !mark) throw RuleViolation("Your mark has not been released yet");
    return *mark;
}

}  // namespace lms::services
