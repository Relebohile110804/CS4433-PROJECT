#pragma once
#include "business/ports/IClock.h"

namespace lms::data {

class SystemClock : public ports::IClock {
public:
    domain::TimePoint now() override { return std::chrono::system_clock::now(); }
};

}  // namespace lms::data
