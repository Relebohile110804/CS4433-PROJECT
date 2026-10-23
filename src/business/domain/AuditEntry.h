#pragma once
#include <optional>
#include <string>

#include "business/domain/Time.h"

namespace lms::domain {

// One creation or change of a mark (FR-19, BR-07). All attributes are read-only:
// there are no setters, and the audit_log table rejects UPDATE and DELETE.
class AuditEntry {
public:
    AuditEntry(int id, int submissionId, int changedBy, TimePoint changedAt, std::optional<double> oldValue,
               double newValue, std::string reason);

    int getId() const { return id_; }
    int getSubmissionId() const { return submissionId_; }
    int getChangedBy() const { return changedBy_; }
    TimePoint getChangedAt() const { return changedAt_; }
    const std::optional<double>& getOldValue() const { return oldValue_; }
    double getNewValue() const { return newValue_; }
    const std::string& getReason() const { return reason_; }

private:
    const int id_;
    const int submissionId_;
    const int changedBy_;
    const TimePoint changedAt_;
    const std::optional<double> oldValue_;
    const double newValue_;
    const std::string reason_;
};

}  // namespace lms::domain
