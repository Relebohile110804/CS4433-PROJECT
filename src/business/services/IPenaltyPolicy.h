#pragma once
#include "business/domain/Time.h"

namespace lms::services {

// Strategy pattern (Section 2.2): the late penalty rule can be swapped without
// changing SubmissionService or MarkingService.
class IPenaltyPolicy {
public:
    virtual ~IPenaltyPolicy() = default;
    // Number of started 24-hour periods after the deadline; 0 if on time.
    virtual int daysLate(domain::TimePoint deadline, domain::TimePoint submittedAt) const = 0;
    virtual double finalMark(int rawMark, int maxMark, int daysLate) const = 0;
};

}  // namespace lms::services
