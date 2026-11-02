#include "presentation/App.h"

#include "business/domain/Errors.h"

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
            default: break;  // menu() only returns listed choices
        }
    }
}

std::string App::studentStatus(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Marked:
        case SubmissionStatus::Returned:
        case SubmissionStatus::Moderated: return "Marked, awaiting release";
        default: return displayName(status);
    }
}

void App::showMyAssignments(const Session& s) {
    std::vector<Assignment> list = s_.assignments.listForStudent(s);
    io_.heading("My assignments (" + std::to_string(list.size()) + ")");
    int me = s.getUser().getId();
    for (const Assignment& a : list) {
        TimePoint due = s_.assignments.effectiveDeadline(a.getId(), me);
        const Extension* ext = a.extensionFor(me);
        std::string extended = ext ? " (extended)" : "";
        auto sub = s_.submissions.findOwn(s, a.getId());
        std::string status = sub ? studentStatus(sub->getStatus()) : "Not submitted";
        // FR-10: the course, the effective deadline and the student's own status.
        io_.say(courseCode(s, a.getCourseId()) + "  " + a.getTitle() + ", out of " + std::to_string(a.getMaxMark()));
        std::string line = "    Due " + ConsoleIO::formatTime(due);
        line += extended;
        line += "   Status: " + status;
        io_.say(line);
        // CR1: show the extension itself, not only the new deadline.
        if (ext)
            io_.say("    Extended from " + ConsoleIO::formatTime(a.getDeadline()) +
                    (ext->isWholeClass() ? " for the whole class: " : " for you: ") + ext->getReason());
    }
}

void App::submitWork(const Session& s) {
    auto a = pickAssignmentFrom(s_.assignments.listForStudent(s));
    if (!a) return;
    if (!canSubmit(s, *a)) return;

    // Step 3 and E1: up to three attempts at a file that can be submitted.
    std::optional<std::string> path = askForFile();
    if (!path) return;

    // Step 4, or A2 when the deadline has passed.
    int late = s_.submissions.previewDaysLate(s, a->getId());
    showLateWarning(late);
    if (!io_.confirm("Submit this file")) {  // A3
        io_.say("Nothing was submitted.");
        return;
    }

    std::optional<SubmissionVersion> v = storeSubmission(s, *a, *path);
    if (!v) return;
    // Step 6: the receipt
    io_.heading("Receipt");
    io_.say("Assignment: " + a->getTitle());
    io_.say("Version " + std::to_string(v->getVersionNo()) + ", handed in " + ConsoleIO::formatTime(v->getSubmittedAt()));
    io_.say(std::string("Status: ") + (late > 0 ? "Late" : "Submitted"));
    io_.say("File check (SHA-256): " + v->getSha256().substr(0, 8));
}

bool App::canSubmit(const Session& s, const Assignment& a) {
    // Step 2: the effective deadline (UC-12), then E2 and E3 before any file is asked for.
    TimePoint due = s_.assignments.effectiveDeadline(a.getId(), s.getUser().getId());
    io_.say("Your deadline: " + ConsoleIO::formatTime(due) + (due != a.getDeadline() ? " (extended)" : ""));
    auto existing = s_.submissions.findOwn(s, a.getId());
    try {
        s_.submissions.checkCanSubmit(s, a.getId());
    } catch (const RuleViolation& e) {
        io_.error(e.what());
        if (existing) io_.say("Status: " + studentStatus(existing->getStatus()));
        return false;
    }
    if (existing) {  // A1
        io_.say("You have already handed in version " + std::to_string(existing->currentVersion().getVersionNo()) + ".");
        io_.say("If you go on, the new file will replace it as the version that is marked.");
        io_.say("The earlier version is kept.");
    }
    return true;
}

std::optional<std::string> App::askForFile() {
    for (int tries = 1; tries <= 3; ++tries) {
        std::string path = io_.requiredText("Full path of your file (.pdf .docx .txt .zip .cpp .h, up to 10 MB)");
        try {
            s_.submissions.validateFile(path);
            return path;
        } catch (const RuleViolation& e) {
            io_.error(e.what());
        }
    }
    io_.say("Three attempts failed. Returning to the menu.");
    return std::nullopt;
}

void App::showLateWarning(int late) {
    int percent = s_.submissions.penaltyPercent(late);
    if (late == 0) {
        io_.say("The submission is on time.");
        return;
    }
    if (percent < 100)
        io_.say("This submission is " + std::to_string(late) + (late == 1 ? " day" : " days") + " late: " +
                std::to_string(percent) + "% of the maximum mark will be deducted.");
    else
        io_.say("This submission is more than 3 days late: the mark will be 0.");
    io_.say("Late work can only be handed in once.");
}

std::optional<SubmissionVersion> App::storeSubmission(const Session& s, const Assignment& a, const std::string& path) {
    try {
        return s_.submissions.submit(s, a.getId(), path);
    } catch (const RuleViolation&) {
        throw;
    } catch (const AccessDenied&) {
        throw;
    } catch (const std::exception& e) {  // E4: nothing was stored
        logError(e);
        io_.error("The work was not submitted because of an error. Please try again.");
        io_.say(std::string("Details: ") + e.what());
        return std::nullopt;
    }
}

void App::showMyMark(const Session& s) {
    auto a = pickAssignmentFrom(s_.assignments.listForStudent(s));
    if (!a) return;
    Mark m = s_.submissions.viewReleasedMark(s, a->getId());
    io_.heading("Mark for " + a->getTitle());
    io_.say("Raw mark: " + std::to_string(m.getRawMark()) + " out of " + std::to_string(a->getMaxMark()));
    io_.say("Late penalty: " + ConsoleIO::formatMark(m.getPenalty()));
    io_.say("Final mark: " + ConsoleIO::formatMark(m.getFinalMark()));
    io_.say("Feedback: " + m.getFeedback());
}

}  // namespace lms::presentation
