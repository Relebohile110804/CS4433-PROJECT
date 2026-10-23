#include "data/SqliteAuditRepository.h"

#include "data/TimeText.h"

namespace lms::data {

using namespace lms::domain;

int SqliteAuditRepository::append(const AuditEntry& e) {
    Statement st = db_.prepare(
        "INSERT INTO audit_log (submission_id, changed_by, changed_at, old_value, new_value, reason) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    st.bindInt(1, e.getSubmissionId())
        .bindInt(2, e.getChangedBy())
        .bindText(3, toIsoText(e.getChangedAt()))
        .bindOptionalDouble(4, e.getOldValue())
        .bindDouble(5, e.getNewValue())
        .bindText(6, e.getReason())
        .run();
    return static_cast<int>(db_.lastInsertId());
}

std::vector<AuditEntry> SqliteAuditRepository::query(std::optional<int> departmentId, std::optional<int> courseId,
                                                     std::optional<int> assignmentId) {
    Statement st = db_.prepare(
        "SELECT a.audit_id, a.submission_id, a.changed_by, a.changed_at, a.old_value, a.new_value, a.reason "
        "FROM audit_log a "
        "JOIN submissions s ON s.submission_id = a.submission_id "
        "JOIN assignments asg ON asg.assignment_id = s.assignment_id "
        "JOIN courses c ON c.course_id = asg.course_id "
        "WHERE (?1 IS NULL OR c.department_id = ?1) AND (?2 IS NULL OR c.course_id = ?2) "
        "AND (?3 IS NULL OR asg.assignment_id = ?3) "
        "ORDER BY a.changed_at, a.audit_id");
    st.bindOptionalInt(1, departmentId).bindOptionalInt(2, courseId).bindOptionalInt(3, assignmentId);
    std::vector<AuditEntry> result;
    while (st.step()) {
        std::optional<double> oldValue;
        if (!st.isNull(4)) oldValue = st.columnDouble(4);
        result.emplace_back(st.columnInt(0), st.columnInt(1), st.columnInt(2), fromIsoText(st.columnText(3)),
                            oldValue, st.columnDouble(5), st.columnText(6));
    }
    return result;
}

}  // namespace lms::data
