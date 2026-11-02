#include "data/SqliteAssignmentRepository.h"

#include "data/TimeText.h"

namespace lms::data {

using namespace lms::domain;

std::vector<Assignment> SqliteAssignmentRepository::load(Statement& st) {
    std::vector<Assignment> result;
    while (st.step())
        result.emplace_back(st.columnInt(0), st.columnInt(1), st.columnText(2), st.columnText(3),
                            fromIsoText(st.columnText(4)), st.columnInt(5));
    for (Assignment& a : result) {
        Statement ext = db_.prepare(
            "SELECT extension_id, student_id, new_deadline, reason, granted_by, granted_at FROM extensions "
            "WHERE assignment_id = ?1 ORDER BY extension_id");
        ext.bindInt(1, a.getId());
        while (ext.step())
            a.restoreExtension(Extension(ext.columnInt(0), ext.columnInt(1), fromIsoText(ext.columnText(2)),
                                         ext.columnText(3), ext.columnInt(4), fromIsoText(ext.columnText(5))));
        Statement cls = db_.prepare(
            "SELECT class_extension_id, new_deadline, reason, granted_by, granted_at FROM class_extensions "
            "WHERE assignment_id = ?1 ORDER BY class_extension_id");
        cls.bindInt(1, a.getId());
        while (cls.step())
            a.restoreExtension(Extension(cls.columnInt(0), Extension::WholeClass, fromIsoText(cls.columnText(1)),
                                         cls.columnText(2), cls.columnInt(3), fromIsoText(cls.columnText(4))));
    }
    return result;
}

std::optional<Assignment> SqliteAssignmentRepository::findById(int id) {
    Statement st = db_.prepare(
        "SELECT assignment_id, course_id, title, description, deadline, max_mark FROM assignments "
        "WHERE assignment_id = ?1");
    st.bindInt(1, id);
    auto found = load(st);
    if (found.empty()) return std::nullopt;
    return found.front();
}

std::vector<Assignment> SqliteAssignmentRepository::forCourse(int courseId) {
    Statement st = db_.prepare(
        "SELECT assignment_id, course_id, title, description, deadline, max_mark FROM assignments "
        "WHERE course_id = ?1 ORDER BY deadline, assignment_id");
    st.bindInt(1, courseId);
    return load(st);
}

int SqliteAssignmentRepository::add(const Assignment& a) {
    Statement st = db_.prepare(
        "INSERT INTO assignments (course_id, title, description, deadline, max_mark) VALUES (?1, ?2, ?3, ?4, ?5)");
    st.bindInt(1, a.getCourseId())
        .bindText(2, a.getTitle())
        .bindText(3, a.getDescription())
        .bindText(4, toIsoText(a.getDeadline()))
        .bindInt(5, a.getMaxMark())
        .run();
    return static_cast<int>(db_.lastInsertId());
}

int SqliteAssignmentRepository::addExtension(int assignmentId, const Extension& e) {
    Statement st = e.isWholeClass()
                       ? db_.prepare("INSERT INTO class_extensions (assignment_id, new_deadline, reason, granted_by, "
                                     "granted_at) VALUES (?1, ?3, ?4, ?5, ?6)")
                       : db_.prepare("INSERT INTO extensions (assignment_id, student_id, new_deadline, reason, "
                                     "granted_by, granted_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    st.bindInt(1, assignmentId);
    if (!e.isWholeClass()) st.bindInt(2, e.getStudentId());
    st.bindText(3, toIsoText(e.getNewDeadline()))
        .bindText(4, e.getReason())
        .bindInt(5, e.getGrantedBy())
        .bindText(6, toIsoText(e.getGrantedAt()))
        .run();
    return static_cast<int>(db_.lastInsertId());
}

}  // namespace lms::data
