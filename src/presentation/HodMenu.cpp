#include "presentation/App.h"

namespace lms::presentation {

using namespace lms::domain;

void App::hodMenu(const Session& s) {
    for (;;) {
        int choice = io_.menu("Head of Department", {"Moderate marks", "Release marks", "Audit log",
                                                     "Change my password"},
                              "Log out");
        switch (choice) {
            case 0: return;
            case 1: attempt([&] { moderate(s); }); break;
            case 2: attempt([&] { releaseMarks(s); }); break;
            case 3: attempt([&] { viewAuditLog(s); }); break;
            case 4: attempt([&] { changePassword(s); }); break;
        }
    }
}

void App::moderate(const Session& s) {
    // Step 1: each assignment with marked work, and how many submissions are in each status.
    std::vector<Assignment> queue = s_.moderation.queue(s);
    if (queue.empty()) {
        io_.say("There are no assignments here.");
        return;
    }
    std::vector<ConsoleIO::Option> rows;
    for (const Assignment& a : queue) {
        int waiting = 0, moderated = 0, returned = 0, released = 0;
        for (const Submission& sub : s_.moderation.listForModeration(s, a.getId())) {
            switch (sub.getStatus()) {
                case SubmissionStatus::Marked: ++waiting; break;
                case SubmissionStatus::Moderated: ++moderated; break;
                case SubmissionStatus::Returned: ++returned; break;
                case SubmissionStatus::Released: ++released; break;
                default: break;
            }
        }
        rows.push_back({a.getTitle() + " (" + courseCode(s, a.getCourseId()) + "), due " +
                            ConsoleIO::formatTime(a.getDeadline()),
                        {std::to_string(waiting) + " awaiting moderation, " + std::to_string(moderated) +
                         " moderated, " + std::to_string(returned) + " returned, " + std::to_string(released) +
                         " released"}});
    }
    int pick = io_.detailedMenu("Choose an assignment", rows, "Cancel");
    if (pick == 0) return;
    const Assignment& a = queue[pick - 1];

    for (;;) {
        // Step 2: marks, feedback, marker and any extension, read again after every action.
        std::vector<Submission> list = s_.moderation.listForModeration(s, a.getId());
        std::vector<ConsoleIO::Option> subs;
        for (const Submission& sub : list) {
            const Mark& m = *sub.getMark();
            ConsoleIO::Option row{s_.auth.nameOf(s, sub.getStudentId()) + "  raw " + std::to_string(m.getRawMark()) +
                                      ", penalty " + ConsoleIO::formatMark(m.getPenalty()) + ", final " +
                                      ConsoleIO::formatMark(m.getFinalMark()) + "  " + displayName(sub.getStatus()),
                                  {"Marked by " + s_.auth.nameOf(s, m.getMarkedBy()) + ": " + m.getFeedback()}};
            for (const Extension& e : a.getExtensions())
                if (e.getStudentId() == sub.getStudentId())
                    row.details.push_back("Extension to " + ConsoleIO::formatTime(e.getNewDeadline()) + ": " +
                                          e.getReason());
            subs.push_back(row);
        }
        int choice = io_.detailedMenu(a.getTitle() + ": choose a submission", subs);
        if (choice == 0) return;
        const Submission& sub = list[choice - 1];

        attempt([&] {
            io_.heading("Submission by " + s_.auth.nameOf(s, sub.getStudentId()));
            io_.say("Feedback: " + sub.getMark()->getFeedback());
            int action = io_.menu("What do you want to do",
                                  {"Approve the mark", "Adjust the mark", "Return to the lecturer"}, "Cancel");
            switch (action) {
                case 1:
                    s_.moderation.approve(s, sub.getId());
                    io_.say("Mark approved.");
                    break;
                case 2: {
                    double mark = io_.decimal("New final mark", 0, a.getMaxMark());
                    s_.moderation.adjust(s, sub.getId(), mark, io_.requiredText("Reason"));
                    io_.say("Mark adjusted.");
                    break;
                }
                case 3:
                    s_.moderation.returnForRemarking(s, sub.getId(), io_.requiredText("Comment for the lecturer"));
                    io_.say("Returned to the lecturer.");
                    break;
            }
        });
    }
}

void App::releaseMarks(const Session& s) {
    auto courseId = pickCourse(s);
    if (!courseId) return;
    auto a = pickAssignment(s, *courseId);
    if (!a) return;
    // Step 4: how many will be released and how many stay hidden, before asking (A3 if none).
    int moderated = 0, notYet = 0;
    for (const Submission& sub : s_.moderation.listForModeration(s, a->getId())) {
        if (sub.getStatus() == SubmissionStatus::Moderated) ++moderated;
        if (sub.getStatus() == SubmissionStatus::Marked || sub.getStatus() == SubmissionStatus::Returned) ++notYet;
    }
    if (moderated == 0) {
        io_.error("There are no moderated marks to release for this assignment");
        return;
    }
    io_.say(std::to_string(moderated) + " moderated mark(s) will be released.");
    if (notYet > 0)
        io_.say(std::to_string(notYet) + " marked submission(s) are not yet moderated and will stay hidden.");
    if (!io_.confirm("Release these marks now")) return;
    int count = s_.moderation.release(s, a->getId());
    io_.say(std::to_string(count) + " mark(s) released.");
}

}  // namespace lms::presentation
