#include "business/domain/AuditEntry.h"

#include "business/domain/Errors.h"

namespace lms::domain {

AuditEntry::AuditEntry(int id, int submissionId, int changedBy, TimePoint changedAt, std::optional<double> oldValue,
                       double newValue, std::string reason)
    : id_(id),
      submissionId_(submissionId),
      changedBy_(changedBy),
      changedAt_(changedAt),
      oldValue_(oldValue),
      newValue_(newValue),
      reason_(std::move(reason)) {
    if (isBlank(reason_)) throw RuleViolation("An audit entry needs a reason");
}

}  // namespace lms::domain
