#pragma once
#include <stdexcept>
#include <string>

namespace lms::domain {

// Thrown when an operation would break a business rule, for example
// approving a submission that has not been marked (Figure 5).
class RuleViolation : public std::runtime_error {
public:
    explicit RuleViolation(const std::string& message) : std::runtime_error(message) {}
};

// Thrown when the logged-in user's role does not allow an operation (NFR-05).
class AccessDenied : public std::runtime_error {
public:
    explicit AccessDenied(const std::string& message) : std::runtime_error(message) {}
};

// True if the text is empty or only spaces, tabs or newlines.
bool isBlank(const std::string& text);

}  // namespace lms::domain
