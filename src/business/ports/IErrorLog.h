#pragma once
#include <string>

namespace lms::ports {

// Where unexpected errors (database or file errors) are written, so the administrator
// can see what went wrong after the user has been told (UC-09 E4, UC-16 E4, UC-20 E2).
class IErrorLog {
public:
    virtual ~IErrorLog() = default;
    virtual void record(const std::string& message) = 0;
};

}  // namespace lms::ports
