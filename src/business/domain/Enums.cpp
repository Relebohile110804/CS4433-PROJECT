#include "business/domain/Enums.h"

#include "business/domain/Errors.h"

namespace lms::domain {

std::string toCode(Role role) {
    switch (role) {
        case Role::Administrator: return "ADMIN";
        case Role::Lecturer: return "LECTURER";
        case Role::HeadOfDepartment: return "HOD";
        case Role::Student: return "STUDENT";
    }
    throw RuleViolation("Unknown role");
}

Role roleFromCode(const std::string& code) {
    if (code == "ADMIN") return Role::Administrator;
    if (code == "LECTURER") return Role::Lecturer;
    if (code == "HOD") return Role::HeadOfDepartment;
    if (code == "STUDENT") return Role::Student;
    throw RuleViolation("Unknown role code: " + code);
}

std::string toCode(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Submitted: return "SUBMITTED";
        case SubmissionStatus::Late: return "LATE";
        case SubmissionStatus::Marked: return "MARKED";
        case SubmissionStatus::Returned: return "RETURNED";
        case SubmissionStatus::Moderated: return "MODERATED";
        case SubmissionStatus::Released: return "RELEASED";
    }
    throw RuleViolation("Unknown submission status");
}

SubmissionStatus statusFromCode(const std::string& code) {
    if (code == "SUBMITTED") return SubmissionStatus::Submitted;
    if (code == "LATE") return SubmissionStatus::Late;
    if (code == "MARKED") return SubmissionStatus::Marked;
    if (code == "RETURNED") return SubmissionStatus::Returned;
    if (code == "MODERATED") return SubmissionStatus::Moderated;
    if (code == "RELEASED") return SubmissionStatus::Released;
    throw RuleViolation("Unknown submission status code: " + code);
}

std::string displayName(Role role) {
    switch (role) {
        case Role::Administrator: return "Administrator";
        case Role::Lecturer: return "Lecturer";
        case Role::HeadOfDepartment: return "Head of Department";
        case Role::Student: return "Student";
    }
    return "";
}

std::string displayName(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Submitted: return "Submitted";
        case SubmissionStatus::Late: return "Late";
        case SubmissionStatus::Marked: return "Awaiting moderation";
        case SubmissionStatus::Returned: return "Returned for re-marking";
        case SubmissionStatus::Moderated: return "Moderated";
        case SubmissionStatus::Released: return "Released";
    }
    return "";
}

}  // namespace lms::domain
