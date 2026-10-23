#pragma once
#include <string>

namespace lms::domain {

enum class Role { Administrator, Lecturer, HeadOfDepartment, Student };

enum class Permission {
    ManageUsers,
    ManageCourses,
    ManageEnrolment,
    BackupRestore,
    CreateAssignment,
    GrantExtension,
    MarkSubmission,
    ModerateMarks,
    ReleaseMarks,
    ViewAuditLog,
    SubmitWork,
    ViewOwnMarks
};

enum class SubmissionStatus { Submitted, Late, Marked, Returned, Moderated, Released };

// Codes stored in the database CHECK constraints (users.role, submissions.status).
std::string toCode(Role role);
Role roleFromCode(const std::string& code);
std::string toCode(SubmissionStatus status);
SubmissionStatus statusFromCode(const std::string& code);

// Words shown on screen, for example "Head of Department" or "Awaiting moderation".
std::string displayName(Role role);
std::string displayName(SubmissionStatus status);

}  // namespace lms::domain
