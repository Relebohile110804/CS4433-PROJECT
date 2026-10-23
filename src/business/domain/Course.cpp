#include "business/domain/Course.h"

#include "business/domain/Errors.h"

namespace lms::domain {

Course::Course(int id, int departmentId, std::string code, std::string title)
    : id_(id), departmentId_(departmentId), code_(std::move(code)), title_(std::move(title)) {
    if (isBlank(code_)) throw RuleViolation("Course code cannot be empty");
    if (isBlank(title_)) throw RuleViolation("Course title cannot be empty");
}

void Course::rename(const std::string& code, const std::string& title) {
    if (isBlank(code)) throw RuleViolation("Course code cannot be empty");
    if (isBlank(title)) throw RuleViolation("Course title cannot be empty");
    code_ = code;
    title_ = title;
}

}  // namespace lms::domain
