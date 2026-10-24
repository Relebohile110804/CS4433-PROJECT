#include "business/services/TenPercentPerDayPolicy.h"

#include <algorithm>

namespace lms::services {

int TenPercentPerDayPolicy::daysLate(domain::TimePoint deadline, domain::TimePoint submittedAt) const {
    if (submittedAt <= deadline) return 0;
    // One second late already counts as one day, so round the late time up to whole days.
    auto late = std::chrono::duration_cast<std::chrono::seconds>(submittedAt - deadline).count();
    const long long day = 24 * 60 * 60;
    return static_cast<int>((late + day - 1) / day);
}

double TenPercentPerDayPolicy::finalMark(int rawMark, int maxMark, int daysLate) const {
    if (daysLate <= 0) return rawMark;
    if (daysLate > MaxDaysLate) return 0;
    return std::max(0.0, rawMark - RatePerDay * daysLate * maxMark);
}

}  // namespace lms::services
