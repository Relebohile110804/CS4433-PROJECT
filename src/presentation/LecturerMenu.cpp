#include "presentation/App.h"

namespace lms::presentation {

using namespace lms::domain;

void App::lecturerMenu(const Session& s) {
    for (;;) {
        int choice = io_.menu("Lecturer", {"Create an assignment", "Grant an extension",
                                           "Submissions and marking", "Change my password"},
                              "Log out");
        switch (choice) {
            case 0: return;
            case 1: attempt([&] { createAssignment(s); }); break;
            case 2: attempt([&] { grantExtension(s); }); break;
            case 3: attempt([&] { markSubmissions(s); }); break;
            case 4: attempt([&] { changePassword(s); }); break;
            default: break;  // menu() only returns listed choices
        }
    }
}

void App::createAssignment(const Session& s) {
    auto course = pickCourse(s);
    if (!course) return;
    std::string title = io_.requiredText("Title");
    std::string description = io_.text("Description");
    TimePoint deadline = io_.dateTime("Deadline, Lesotho time");
    int maxMark = io_.number("Maximum mark", 1, 1000);
    s_.assignments.createAssignment(s, *course, title, description, deadline, maxMark);
    io_.say("Assignment created.");
}

void App::grantExtension(const Session& s) {
    auto courseId = pickCourse(s);
    if (!courseId) return;
    auto a = pickAssignment(s, *courseId);
    if (!a) return;

    std::vector<Course> courses = s_.assignments.myCourses(s);
    std::vector<int> students;
    for (const Course& c : courses)
        if (c.getId() == *courseId) students.assign(c.getStudentIds().begin(), c.getStudentIds().end());
    std::vector<std::string> rows;
    rows.reserve(students.size());
    for (int id : students)
        rows.push_back(s_.auth.nameOf(s, id) + "  due " +
                       ConsoleIO::formatTime(s_.assignments.effectiveDeadline(a->getId(), id)));
    if (rows.empty()) {
        io_.say("No students are enrolled in this course.");
        return;
    }
    int choice = io_.menu("Choose a student", rows, "Cancel");
    if (choice == 0) return;

    TimePoint newDeadline = io_.dateTime("New deadline, Lesotho time");
    std::string reason = io_.requiredText("Reason");
    s_.assignments.grantExtension(s, a->getId(), students[choice - 1], newDeadline, reason);
    io_.say("Extension granted.");
}

void App::markSubmissions(const Session& s) {
    auto courseId = pickCourse(s);
    if (!courseId) return;
    auto a = pickAssignment(s, *courseId);
    if (!a) return;

    for (;;) {
        // FR-14, FR-22: every submission with its current status.
        std::vector<Submission> list = s_.marking.listSubmissions(s, a->getId());
        if (list.empty()) {
            io_.say("Nothing has been submitted yet.");
            return;
        }
        std::vector<ConsoleIO::Option> rows;
        rows.reserve(list.size());
        for (const Submission& sub : list) rows.push_back(markingRow(s, sub));
        int choice = io_.detailedMenu(a->getTitle() + ": choose a submission to mark", rows);
        if (choice == 0) return;
        const Submission& sub = list[choice - 1];

        // UC-16 E3: after moderation only the HoD can change the mark.
        if (sub.getStatus() == SubmissionStatus::Moderated || sub.getStatus() == SubmissionStatus::Released) {
            io_.error("This mark is " + displayName(sub.getStatus()) + ", so it can no longer be changed here.");
            io_.say("Please contact the Head of Department.");
            continue;
        }
        attempt([&] { markOne(s, *a, sub); });
    }
}

ConsoleIO::Option App::markingRow(const Session& s, const Submission& sub) {
    // FR-14: student, status and days late, with the time received on the line below.
    std::string row = s_.auth.nameOf(s, sub.getStudentId()) + "  " + displayName(sub.getStatus());
    if (sub.getDaysLate() > 0) row += ", " + std::to_string(sub.getDaysLate()) + " day(s) late";
    if (const std::optional<Mark>& mark = sub.getMark()) row += ", final " + ConsoleIO::formatMark(mark->getFinalMark());
    return {row, {"handed in " + ConsoleIO::formatTime(sub.currentVersion().getSubmittedAt())}};
}

void App::markOne(const Session& s, const Assignment& a, const Submission& sub) {
    const SubmissionVersion& v = sub.currentVersion();
    const std::optional<Mark>& current = sub.getMark();
    io_.heading("Submission by " + s_.auth.nameOf(s, sub.getStudentId()));
    io_.say("File: " + v.getStoredPath());
    io_.say("Version " + std::to_string(v.getVersionNo()) + ", handed in " + ConsoleIO::formatTime(v.getSubmittedAt()) +
            ", SHA-256 " + v.getSha256().substr(0, 8));
    if (!sub.getHodComment().empty()) io_.say("HoD comment: " + sub.getHodComment());
    if (current) io_.say("Current final mark: " + ConsoleIO::formatMark(current->getFinalMark()));

    int raw = io_.number("Raw mark", 0, a.getMaxMark());
    services::MarkPreview p = s_.marking.previewMark(s, sub.getId(), raw);
    io_.say("Days late: " + std::to_string(p.daysLate) + "   Penalty: " + ConsoleIO::formatMark(p.penalty) +
            "   Final mark: " + ConsoleIO::formatMark(p.finalMark));
    std::string feedback = io_.requiredText("Feedback");
    std::string reason = current ? io_.requiredText("Reason for changing the mark") : "";
    if (!io_.confirm("Save this mark")) {  // UC-16 A3
        io_.say("Nothing was saved.");
        return;
    }
    s_.marking.mark(s, sub.getId(), raw, feedback, reason);
    io_.say("Mark saved. It now waits for the Head of Department.");
}

}  // namespace lms::presentation
