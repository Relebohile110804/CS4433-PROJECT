#include "data/SqliteSubmissionRepository.h"

#include "data/TimeText.h"

namespace lms::data {

using namespace lms::domain;

namespace {

const char* const SelectSubmission =
    "SELECT submission_id, assignment_id, student_id, status, days_late, moderated_by, moderated_at, released_at, "
    "hod_comment FROM submissions ";

std::optional<TimePoint> optionalTime(const Statement& st, int col) {
    if (st.isNull(col)) return std::nullopt;
    return fromIsoText(st.columnText(col));
}

void bindOptionalTime(Statement& st, int index, const std::optional<TimePoint>& t) {
    if (t)
        st.bindText(index, toIsoText(*t));
    else
        st.bindNull(index);
}

}  // namespace

std::vector<Submission> SqliteSubmissionRepository::load(Statement& st) {
    std::vector<Submission> result;
    while (st.step()) {
        int id = st.columnInt(0);

        std::vector<SubmissionVersion> versions;
        Statement ver = db_.prepare(
            "SELECT version_no, stored_path, sha256, submitted_at FROM submission_versions "
            "WHERE submission_id = ?1 ORDER BY version_no");
        ver.bindInt(1, id);
        while (ver.step())
            versions.emplace_back(ver.columnInt(0), ver.columnText(1), ver.columnText(2),
                                  fromIsoText(ver.columnText(3)));

        std::optional<Mark> mark;
        Statement mk = db_.prepare(
            "SELECT raw_mark, penalty, final_mark, feedback, marked_by, marked_at FROM marks WHERE submission_id = ?1");
        mk.bindInt(1, id);
        if (mk.step())
            mark.emplace(mk.columnInt(0), mk.columnDouble(1), mk.columnDouble(2), mk.columnText(3), mk.columnInt(4),
                         fromIsoText(mk.columnText(5)));

        std::optional<int> moderatedBy;
        if (!st.isNull(5)) moderatedBy = st.columnInt(5);

        result.emplace_back(id, st.columnInt(1), st.columnInt(2), statusFromCode(st.columnText(3)), st.columnInt(4),
                            std::move(versions), std::move(mark), moderatedBy, optionalTime(st, 6),
                            optionalTime(st, 7), st.columnText(8));
    }
    return result;
}

std::optional<Submission> SqliteSubmissionRepository::findById(int id) {
    Statement st = db_.prepare(std::string(SelectSubmission) + "WHERE submission_id = ?1");
    st.bindInt(1, id);
    auto found = load(st);
    if (found.empty()) return std::nullopt;
    return found.front();
}

std::optional<Submission> SqliteSubmissionRepository::find(int assignmentId, int studentId) {
    Statement st = db_.prepare(std::string(SelectSubmission) + "WHERE assignment_id = ?1 AND student_id = ?2");
    st.bindInt(1, assignmentId).bindInt(2, studentId);
    auto found = load(st);
    if (found.empty()) return std::nullopt;
    return found.front();
}

std::vector<Submission> SqliteSubmissionRepository::forAssignment(int assignmentId) {
    Statement st = db_.prepare(std::string(SelectSubmission) + "WHERE assignment_id = ?1 ORDER BY submission_id");
    st.bindInt(1, assignmentId);
    return load(st);
}

std::vector<Submission> SqliteSubmissionRepository::forStudent(int studentId) {
    Statement st = db_.prepare(std::string(SelectSubmission) + "WHERE student_id = ?1 ORDER BY submission_id");
    st.bindInt(1, studentId);
    return load(st);
}

int SqliteSubmissionRepository::save(const Submission& s) {
    // The row, its new versions and its mark are saved together or not at all.
    auto tx = db_.begin();

    int id = s.getId();
    if (id == 0) {
        Statement ins = db_.prepare(
            "INSERT INTO submissions (assignment_id, student_id, status, days_late, moderated_by, moderated_at, "
            "released_at, hod_comment) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
        ins.bindInt(1, s.getAssignmentId()).bindInt(2, s.getStudentId());
        ins.bindText(3, toCode(s.getStatus())).bindInt(4, s.getDaysLate()).bindOptionalInt(5, s.getModeratedBy());
        bindOptionalTime(ins, 6, s.getModeratedAt());
        bindOptionalTime(ins, 7, s.getReleasedAt());
        if (s.getHodComment().empty())
            ins.bindNull(8);
        else
            ins.bindText(8, s.getHodComment());
        ins.run();
        id = static_cast<int>(db_.lastInsertId());
    } else {
        Statement upd = db_.prepare(
            "UPDATE submissions SET status = ?2, days_late = ?3, moderated_by = ?4, moderated_at = ?5, "
            "released_at = ?6, hod_comment = ?7 WHERE submission_id = ?1");
        upd.bindInt(1, id).bindText(2, toCode(s.getStatus())).bindInt(3, s.getDaysLate());
        upd.bindOptionalInt(4, s.getModeratedBy());
        bindOptionalTime(upd, 5, s.getModeratedAt());
        bindOptionalTime(upd, 6, s.getReleasedAt());
        if (s.getHodComment().empty())
            upd.bindNull(7);
        else
            upd.bindText(7, s.getHodComment());
        upd.run();
    }

    // Versions are never changed once stored, so only new ones are inserted.
    Statement maxVer = db_.prepare("SELECT coalesce(max(version_no), 0) FROM submission_versions WHERE submission_id = ?1");
    maxVer.bindInt(1, id);
    maxVer.step();
    int stored = maxVer.columnInt(0);
    for (const SubmissionVersion& v : s.getVersions()) {
        if (v.getVersionNo() <= stored) continue;
        Statement ins = db_.prepare(
            "INSERT INTO submission_versions (submission_id, version_no, stored_path, sha256, submitted_at) "
            "VALUES (?1, ?2, ?3, ?4, ?5)");
        ins.bindInt(1, id)
            .bindInt(2, v.getVersionNo())
            .bindText(3, v.getStoredPath())
            .bindText(4, v.getSha256())
            .bindText(5, toIsoText(v.getSubmittedAt()))
            .run();
    }

    if (const auto& m = s.getMark()) {
        Statement mk = db_.prepare(
            "INSERT INTO marks (submission_id, raw_mark, penalty, final_mark, feedback, marked_by, marked_at) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7) "
            "ON CONFLICT (submission_id) DO UPDATE SET raw_mark = excluded.raw_mark, penalty = excluded.penalty, "
            "final_mark = excluded.final_mark, feedback = excluded.feedback, marked_by = excluded.marked_by, "
            "marked_at = excluded.marked_at");
        mk.bindInt(1, id)
            .bindInt(2, m->getRawMark())
            .bindDouble(3, m->getPenalty())
            .bindDouble(4, m->getFinalMark())
            .bindText(5, m->getFeedback())
            .bindInt(6, m->getMarkedBy())
            .bindText(7, toIsoText(m->getMarkedAt()))
            .run();
    }

    tx->commit();
    return id;
}

}  // namespace lms::data
