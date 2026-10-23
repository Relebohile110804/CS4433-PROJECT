#include "business/domain/Errors.h"

namespace lms::domain {

bool isBlank(const std::string& text) { return text.find_first_not_of(" \t\r\n") == std::string::npos; }

}  // namespace lms::domain
