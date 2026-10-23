#pragma once
#include <optional>
#include <string>
#include <vector>

#include "business/domain/Enums.h"
#include "business/domain/Time.h"

namespace lms::domain {

// One file handed in for a submission (FR-11, FR-12). Earlier versions are kept read-only.
class SubmissionVersion {
public:
    SubmissionVersion(int versionNo, std::string storedPath, std::string sha256, TimePoint submittedAt);

    int getVersionNo() const { return versionNo_; }
    const std::string& getStoredPath() const { return storedPath_; }
    const std::string& getSha256() const { return sha256_; }
    TimePoint getSubmittedAt() const { return submittedAt_; }

private:
    int versionNo_;
    std::string storedPath_;
    std::string sha256_;
    TimePoint submittedAt_;
};

// The mark and feedback for a submission (FR-15). The penalty is kept as applied
// at marking time; the final mark can differ from raw - penalty after an HoD adjustment.
class Mark {
public:
    Mark(int rawMark, double penalty, double finalMark, std::string feedback, int markedBy, TimePoint markedAt);

    void setFinalMark(double value);

    int getRawMark() const { return rawMark_; }
    double getPenalty() const { return penalty_; }
    double getFinalMark() const { return finalMark_; }
    const std::string& getFeedback() const { return feedback_; }
    int getMarkedBy() const { return markedBy_; }
    TimePoint getMarkedAt() const { return markedAt_; }

private:
    int rawMark_;
    double penalty_;
    double finalMark_;
    std::string feedback_;
    int markedBy_;
    TimePoint markedAt_;
};

// One student's work for one assignment. Its operations only allow the
// transitions of the state machine in Figure 5; anything else throws RuleViolation.
class Submission {
public:
    // A new submission: status is Submitted if on time, otherwise Late (BR-02).
    static Submission createNew(int assignmentId, int studentId, const SubmissionVersion& first, int daysLate);

    // Rebuilds a stored submission (used by the repository).
    Submission(int id, int assignmentId, int studentId, SubmissionStatus status, int daysLate,
               std::vector<SubmissionVersion> versions, std::optional<Mark> mark, std::optional<int> moderatedBy,
               std::optional<TimePoint> moderatedAt, std::optional<TimePoint> releasedAt, std::string hodComment);

    const SubmissionVersion& currentVersion() const;
    // Resubmission (BR-04): only while Submitted and before the effective deadline (daysLate == 0).
    void addVersion(const SubmissionVersion& v, int daysLate);
    // Initial marking, a correction before moderation, or re-marking after a return.
    void recordMark(const Mark& m);
    void approve(int hodId, TimePoint at);
    // HoD changes the final mark: from Marked (moderation) or Released (BR-06).
    void adjust(double newFinal, int hodId, TimePoint at);
    void returnToLecturer(const std::string& comment);
    void release(TimePoint at);

    bool isMarked() const { return mark_.has_value(); }
    // Marks and feedback are visible to the student only after release (BR-05).
    bool isVisibleToStudent() const { return status_ == SubmissionStatus::Released; }

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    int getAssignmentId() const { return assignmentId_; }
    int getStudentId() const { return studentId_; }
    SubmissionStatus getStatus() const { return status_; }
    int getDaysLate() const { return daysLate_; }
    const std::vector<SubmissionVersion>& getVersions() const { return versions_; }
    const std::optional<Mark>& getMark() const { return mark_; }
    const std::optional<int>& getModeratedBy() const { return moderatedBy_; }
    const std::optional<TimePoint>& getModeratedAt() const { return moderatedAt_; }
    const std::optional<TimePoint>& getReleasedAt() const { return releasedAt_; }
    const std::string& getHodComment() const { return hodComment_; }

private:
    int id_;
    int assignmentId_;
    int studentId_;
    SubmissionStatus status_;
    int daysLate_;
    std::vector<SubmissionVersion> versions_;
    std::optional<Mark> mark_;
    std::optional<int> moderatedBy_;
    std::optional<TimePoint> moderatedAt_;
    std::optional<TimePoint> releasedAt_;
    std::string hodComment_;
};

}  // namespace lms::domain
