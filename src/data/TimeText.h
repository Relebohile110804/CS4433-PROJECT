#pragma once
#include <optional>
#include <string>

#include "business/domain/Time.h"

namespace lms::data {

// Times are stored as ISO 8601 text in UTC, for example "2026-11-02T08:30:00Z" (BR-08).
// Precision is whole seconds.
std::string toIsoText(domain::TimePoint t);
domain::TimePoint fromIsoText(const std::string& text);

}  // namespace lms::data
