#pragma once
#include <chrono>

namespace lms::domain {

// All times are UTC points on the system clock (BR-08).
using TimePoint = std::chrono::system_clock::time_point;

}  // namespace lms::domain
