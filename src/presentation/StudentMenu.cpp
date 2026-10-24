#include "presentation/App.h"

namespace lms::presentation {

using namespace lms::domain;

void App::studentMenu(const Session& s) {
    for (;;) {
        int choice = io_.menu("Student", {"My assignments", "Submit work", "My marks", "Change my password"},
                              "Log out");
        switch (choice) {
            case 0: return;
            case 1: attempt([&] { showMyAssignments(s); }); break;
            case 2: attempt([&] { submitWork(s); }); break;
            case 3: attempt([&] { showMyMark(s); }); break;
            case 4: attempt([&] { changePassword(s); }); break;
        }
    }
}

void App::showMyAssignments(const Session& s) {
    std::vector<Assignment> list = s_.assignments.listForStudent(s);
    io_.heading("My assignments (" + std::to_string(list.size()) + ")");
    int me = s.getUser().getId();
    for (const Assignment& a : list) {
        TimePoint due = s_.assignments.effectiveDeadline(a.getId(), me);
        std::string extended = due != a.getDeadline() ? " (extended)" : "";
        auto sub = s_.submissions.findOwn(s, a.getId());
        std::string status = sub ? displayName(sub->getStatus()) : "Not submitted";
        io_.say(ConsoleIO::fit(a.getTitle() + ", out of " + std::to_string(a.getMaxMark())));
        io_.say(ConsoleIO::fit("    Due " + ConsoleIO::formatTime(due) + extended + "   Status: " + status));
    }
}

void App::submitWork(const Session& s) {
    auto a = pickAssignmentFrom(s_.assignments.listForStudent(s));
    if (!a) return;
    int late = s_.submissions.previewDaysLate(s, a->getId());
    if (late > 0) {
        io_.say("The deadline has passed. This submission will be " + std::to_string(late) +
                " day(s) late and a penalty will apply.");
        io_.say("Late work can only be handed in once.");
    }
    std::string path = io_.requiredText("Full path of your file (.pdf .docx .txt .zip .cpp .h, up to 10 MB)");
    if (!io_.confirm("Submit this file")) return;
    SubmissionVersion v = s_.submissions.submit(s, a->getId(), path);
    // Receipt (UC-09 step 8)
    io_.heading("Receipt");
    io_.say("Assignment: " + a->getTitle());
    io_.say("Version " + std::to_string(v.getVersionNo()) + ", handed in " + ConsoleIO::formatTime(v.getSubmittedAt()));
    io_.say("File check (SHA-256): " + v.getSha256().substr(0, 8));
}

void App::showMyMark(const Session& s) {
    auto a = pickAssignmentFrom(s_.assignments.listForStudent(s));
    if (!a) return;
    Mark m = s_.submissions.viewReleasedMark(s, a->getId());
    io_.heading("Mark for " + a->getTitle());
    io_.say("Raw mark: " + std::to_string(m.getRawMark()) + " out of " + std::to_string(a->getMaxMark()));
    io_.say("Late penalty: " + ConsoleIO::formatMark(m.getPenalty()));
    io_.say("Final mark: " + ConsoleIO::formatMark(m.getFinalMark()));
    io_.say(ConsoleIO::fit("Feedback: " + m.getFeedback()));
}

}  // namespace lms::presentation
