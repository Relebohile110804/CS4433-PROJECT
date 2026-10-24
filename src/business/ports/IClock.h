#pragma once
#include "business/domain/Time.h"

namespace lms::ports {

// The only source of the current time (BR-08). Tests use a fake clock so that
// deadline rules can be checked to the second.
class IClock {
public:
    virtual ~IClock() = default;
    virtual domain::TimePoint now() = 0;
};

}  // namespace lms::ports
