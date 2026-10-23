#include "business/domain/Submission.h"

#include <cctype>

#include "business/domain/Errors.h"

namespace lms::domain {

namespace {

void refuse(SubmissionStatus current, const std::string& action) {
    throw RuleViolation("Cannot " + action + " a submission that is " + displayName(current));
}

bool isSha256Hex(const std::string& text) {
    if (text.size() != 64) return false;
    for (char c : text)
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

}  // namespace

SubmissionVersion::SubmissionVersion(int versionNo, std::string storedPath, std::string sha256, TimePoint submittedAt)
    : versionNo_(versionNo), storedPath_(std::move(storedPath)), sha256_(std::move(sha256)), submittedAt_(submittedAt) {
    if (versionNo_ < 1) throw RuleViolation("Version numbers start at 1");
    if (storedPath_.empty()) throw RuleViolation("A version needs a stored file");
    if (!isSha256Hex(sha256_)) throw RuleViolation("A version needs a 64-character SHA-256 hash");
}

Mark::Mark(int rawMark, double penalty, double finalMark, std::string feedback, int markedBy, TimePoint markedAt)
    : rawMark_(rawMark),
      penalty_(penalty),
      finalMark_(finalMark),
      feedback_(std::move(feedback)),
      markedBy_(markedBy),
      markedAt_(markedAt) {
    if (rawMark_ < 0) throw RuleViolation("A mark cannot be negative");
    if (penalty_ < 0) throw RuleViolation("A penalty cannot be negative");
    if (finalMark_ < 0) throw RuleViolation("A final mark cannot be negative");
    if (isBlank(feedback_)) throw RuleViolation("Feedback cannot be empty");
}

void Mark::setFinalMark(double value) {
    if (value < 0) throw RuleViolation("A final mark cannot be negative");
    finalMark_ = value;
}

Submission Submission::createNew(int assignmentId, int studentId, const SubmissionVersion& first, int daysLate) {
    if (first.getVersionNo() != 1) throw RuleViolation("The first version must be version 1");
    if (daysLate < 0) throw RuleViolation("Days late cannot be negative");
    SubmissionStatus status = daysLate == 0 ? SubmissionStatus::Submitted : SubmissionStatus::Late;
    return Submission(0, assignmentId, studentId, status, daysLate, {first}, std::nullopt, std::nullopt,
                      std::nullopt, std::nullopt, "");
}

Submission::Submission(int id, int assignmentId, int studentId, SubmissionStatus status, int daysLate,
                       std::vector<SubmissionVersion> versions, std::optional<Mark> mark,
                       std::optional<int> moderatedBy, std::optional<TimePoint> moderatedAt,
                       std::optional<TimePoint> releasedAt, std::string hodComment)
    : id_(id),
      assignmentId_(assignmentId),
      studentId_(studentId),
      status_(status),
      daysLate_(daysLate),
      versions_(std::move(versions)),
      mark_(std::move(mark)),
      moderatedBy_(moderatedBy),
      moderatedAt_(moderatedAt),
      releasedAt_(releasedAt),
      hodComment_(std::move(hodComment)) {
    if (versions_.empty()) throw RuleViolation("A submission needs at least one version");
}

const SubmissionVersion& Submission::currentVersion() const { return versions_.back(); }

void Submission::addVersion(const SubmissionVersion& v, int daysLate) {
    if (status_ == SubmissionStatus::Late)
        throw RuleViolation("A late submission cannot be replaced (BR-02)");
    if (status_ != SubmissionStatus::Submitted) refuse(status_, "resubmit");
    if (daysLate > 0)
        throw RuleViolation("The deadline has passed, so the version already submitted stands (BR-02)");
    if (v.getVersionNo() != currentVersion().getVersionNo() + 1)
        throw RuleViolation("The new version must be numbered " + std::to_string(currentVersion().getVersionNo() + 1));
    versions_.push_back(v);
}

void Submission::recordMark(const Mark& m) {
    switch (status_) {
        case SubmissionStatus::Submitted:
        case SubmissionStatus::Late:
        case SubmissionStatus::Marked:
        case SubmissionStatus::Returned:
            mark_ = m;
            status_ = SubmissionStatus::Marked;
            return;
        default:
            refuse(status_, "mark");
    }
}

void Submission::approve(int hodId, TimePoint at) {
    if (status_ != SubmissionStatus::Marked) refuse(status_, "approve");
    moderatedBy_ = hodId;
    moderatedAt_ = at;
    status_ = SubmissionStatus::Moderated;
}

void Submission::adjust(double newFinal, int hodId, TimePoint at) {
    if (status_ != SubmissionStatus::Marked && status_ != SubmissionStatus::Released) refuse(status_, "adjust");
    mark_->setFinalMark(newFinal);
    moderatedBy_ = hodId;
    moderatedAt_ = at;
    if (status_ == SubmissionStatus::Marked) status_ = SubmissionStatus::Moderated;
}

void Submission::returnToLecturer(const std::string& comment) {
    if (status_ != SubmissionStatus::Marked) refuse(status_, "return");
    if (isBlank(comment)) throw RuleViolation("A comment is needed when returning a mark");
    hodComment_ = comment;
    status_ = SubmissionStatus::Returned;
}

void Submission::release(TimePoint at) {
    if (status_ != SubmissionStatus::Moderated) refuse(status_, "release");
    releasedAt_ = at;
    status_ = SubmissionStatus::Released;
}

}  // namespace lms::domain
