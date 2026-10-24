#include "business/services/AuditService.h"

#include "business/domain/Errors.h"

namespace lms::services {

using namespace lms::domain;

void AuditService::record(int submissionId, int changedBy, std::optional<double> oldValue, double newValue,
                          const std::string& reason) {
    entries_.append(AuditEntry(0, submissionId, changedBy, clock_.now(), oldValue, newValue, reason));
}

std::vector<AuditEntry> AuditService::viewLog(const Session& s, std::optional<int> courseId,
                                              std::optional<int> assignmentId) {
    s.require(Permission::ViewAuditLog);
    std::optional<int> departmentId;
    if (s.getUser().getRole() == Role::HeadOfDepartment) {
        std::optional<Department> d = courses_.departmentOfHod(s.getUser().getId());
        if (!d) throw AccessDenied("You are not the head of any department");
        departmentId = d->getId();
        if (courseId && (!courses_.findCourse(*courseId) || courses_.departmentOf(*courseId) != d->getId()))
            throw AccessDenied("That course is not in your department");
    }
    return entries_.query(departmentId, courseId, assignmentId);
}

}  // namespace lms::services
