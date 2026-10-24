#pragma once
#include "business/services/IPenaltyPolicy.h"

namespace lms::services {

// BR-01: 10% of the maximum mark for each started day late, up to 3 days; after that the mark is 0.
class TenPercentPerDayPolicy : public IPenaltyPolicy {
public:
    static constexpr double RatePerDay = 0.10;
    static constexpr int MaxDaysLate = 3;

    int daysLate(domain::TimePoint deadline, domain::TimePoint submittedAt) const override;
    double finalMark(int rawMark, int maxMark, int daysLate) const override;
};

}  // namespace lms::services
