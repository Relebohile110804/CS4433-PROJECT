#pragma once
#include <string>

#include "business/domain/Time.h"

namespace testutil {

// A fixed starting time so tests do not depend on the real clock.
inline lms::domain::TimePoint base() { return lms::domain::TimePoint{} + std::chrono::hours(24 * 365 * 56); }
inline lms::domain::TimePoint hoursAfter(double h) {
    return base() + std::chrono::duration_cast<lms::domain::TimePoint::duration>(std::chrono::duration<double, std::ratio<3600>>(h));
}
inline std::string hash(char c = 'a') { return std::string(64, c); }

}  // namespace testutil
