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
        std::vector<std::string> rows;
        for (const Submission& sub : list) {
            std::string row = s_.auth.nameOf(s, sub.getStudentId()) + "  " + displayName(sub.getStatus());
            if (sub.getDaysLate() > 0) row += ", " + std::to_string(sub.getDaysLate()) + " day(s) late";
            if (sub.getMark()) row += ", final " + ConsoleIO::formatMark(sub.getMark()->getFinalMark());
            rows.push_back(row);
        }
        int choice = io_.menu(a->getTitle() + ": choose a submission to mark", rows);
        if (choice == 0) return;
        const Submission& sub = list[choice - 1];

        attempt([&] {
            const SubmissionVersion& v = sub.currentVersion();
            io_.heading("Submission by " + s_.auth.nameOf(s, sub.getStudentId()));
            io_.say(ConsoleIO::fit("File: " + v.getStoredPath()));
            io_.say("Version " + std::to_string(v.getVersionNo()) + ", handed in " +
                    ConsoleIO::formatTime(v.getSubmittedAt()) + ", SHA-256 " + v.getSha256().substr(0, 8));
            if (!sub.getHodComment().empty()) io_.say(ConsoleIO::fit("HoD comment: " + sub.getHodComment()));
            if (sub.getMark()) io_.say("Current final mark: " + ConsoleIO::formatMark(sub.getMark()->getFinalMark()));

            int raw = io_.number("Raw mark", 0, a->getMaxMark());
            services::MarkPreview p = s_.marking.previewMark(s, sub.getId(), raw);
            io_.say("Days late: " + std::to_string(p.daysLate) + "   Penalty: " + ConsoleIO::formatMark(p.penalty) +
                    "   Final mark: " + ConsoleIO::formatMark(p.finalMark));
            std::string feedback = io_.requiredText("Feedback");
            std::string reason = sub.getMark() ? io_.requiredText("Reason for changing the mark") : "";
            if (!io_.confirm("Save this mark")) return;
            s_.marking.mark(s, sub.getId(), raw, feedback, reason);
            io_.say("Mark saved. It now waits for the Head of Department.");
        });
    }
}

}  // namespace lms::presentation
