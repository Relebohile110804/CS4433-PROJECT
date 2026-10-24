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
    // The queue shows only assignments with work awaiting moderation.
    auto a = pickAssignmentFrom(s_.moderation.queue(s));
    if (!a) return;

    for (;;) {
        std::vector<Submission> list = s_.moderation.listForModeration(s, a->getId());
        std::vector<std::string> rows;
        for (const Submission& sub : list) {
            const Mark& m = *sub.getMark();
            rows.push_back(s_.auth.nameOf(s, sub.getStudentId()) + "  raw " + std::to_string(m.getRawMark()) +
                           ", penalty " + ConsoleIO::formatMark(m.getPenalty()) + ", final " +
                           ConsoleIO::formatMark(m.getFinalMark()) + "  " + displayName(sub.getStatus()));
        }
        int choice = io_.menu(a->getTitle() + ": choose a submission", rows);
        if (choice == 0) return;
        const Submission& sub = list[choice - 1];

        attempt([&] {
            io_.heading("Submission by " + s_.auth.nameOf(s, sub.getStudentId()));
            io_.say(ConsoleIO::fit("Feedback: " + sub.getMark()->getFeedback()));
            int action = io_.menu("What do you want to do",
                                  {"Approve the mark", "Adjust the mark", "Return to the lecturer"}, "Cancel");
            switch (action) {
                case 1:
                    s_.moderation.approve(s, sub.getId());
                    io_.say("Mark approved.");
                    break;
                case 2: {
                    double mark = io_.decimal("New final mark", 0, a->getMaxMark());
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
    if (!io_.confirm("Release all moderated marks for " + a->getTitle() + " to the students")) return;
    int count = s_.moderation.release(s, a->getId());
    io_.say(std::to_string(count) + " mark(s) released.");
}

}  // namespace lms::presentation
