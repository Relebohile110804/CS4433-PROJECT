// Acceptance tests AT-01 to AT-07. Each is a scenario written from one stakeholder's
// point of view (Week 2 stakeholder analysis), run on the whole program with the
// developer acting as each user. They check the interest behind the requirements,
// not one requirement at a time.
#include "SystemFixture.h"
#include "business/domain/Errors.h"

using namespace systest;

namespace {

std::string login(const std::string& user) {
    return "1\n" + user + "\n" + (user == "admin" ? "admin-pass" : "password1") + "\n";
}

std::string studentList(SystemFixture& f, const std::string& who) { return f.run(login(who) + "1\n0\n0\n"); }

}  // namespace

TEST_CASE("AT-01: Principal, no student sees a mark or feedback before the HoD has checked and released it",
          "[acceptance]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    CHECK(contains(studentList(f, "ann"), "Status: Submitted"));

    int id = f.submission(f.ann).getId();
    f.marking.mark(f.as("lect"), id, 74, "Secret feedback: very good design", "");
    // After marking, after a return and after approval the student sees only that it is marked.
    std::string marked = studentList(f, "ann") + f.run(login("ann") + "3\n1\n0\n0\n");
    f.moderation.returnForRemarking(f.as("hod"), id, "Check the design mark");
    std::string returned = studentList(f, "ann");
    f.marking.mark(f.as("lect"), id, 72, "Secret feedback: good design", "Agreed with HoD");
    f.moderation.approve(f.as("hod"), id);
    std::string moderated = studentList(f, "ann") + f.run(login("ann") + "3\n1\n0\n0\n");
    for (const std::string& screen : {marked, returned, moderated}) {
        CHECK(contains(screen, "Status: Marked, awaiting release"));
        CHECK_FALSE(contains(screen, "Secret feedback"));
        CHECK_FALSE(contains(screen, "74"));
        CHECK_FALSE(contains(screen, "72"));
    }
    CHECK(contains(moderated, "! Your mark has not been released yet"));
    CHECK_FALSE(f.submitting.findOwn(f.as("ann"), f.project)->getMark());

    f.moderation.release(f.as("hod"), f.project);
    std::string released = f.run(login("ann") + "3\n1\n0\n0\n");
    CHECK(contains(released, "Final mark: 72"));
    CHECK(contains(released, "Feedback: Secret feedback: good design"));
}

TEST_CASE("AT-02: Principal, the system is simple: every function is within 3 menu selections", "[acceptance]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    struct Path {
        std::string user, selections, screen;
    };
    // Each path is typed after logging in; the screen text shows the function has started.
    std::vector<Path> paths = {
        {"admin", "1\n1\n", "== Role =="},              {"admin", "1\n2\n", "== All users"},
        {"admin", "1\n3\n", "== Choose a user =="},     {"admin", "1\n4\n", "== Choose a user =="},
        {"admin", "1\n5\n", "== Choose a user =="},     {"admin", "2\n1\n", "Department name"},
        {"admin", "2\n2\n", "== Choose a department =="}, {"admin", "2\n3\n", "== Choose a department =="},
        {"admin", "2\n4\n", "== Choose a course =="},   {"admin", "2\n5\n", "== Computer Science, HoD"},
        {"admin", "3\n1\n", "== Choose a course =="},   {"admin", "3\n2\n", "== Choose a course =="},
        {"admin", "3\n3\n", "== Choose a course =="},   {"admin", "3\n4\n", "== Choose a course =="},
        {"admin", "4\n1\n", "Backup file name"},        {"admin", "4\n2\n", "Backup file to restore"},
        {"admin", "5\n1\n", "== Audit log ("},          {"admin", "6\n", "Current password"},
        {"lect", "1\n", "== Choose a course =="},       {"lect", "2\n", "== Choose a course =="},
        {"lect", "3\n", "== Choose a course =="},       {"lect", "4\n", "Current password"},
        {"hod", "1\n", "== Choose an assignment =="},   {"hod", "2\n", "== Choose a course =="},
        {"hod", "3\n1\n", "== Audit log ("},            {"hod", "4\n", "Current password"},
        {"ann", "1\n", "== My assignments"},            {"ann", "2\n", "== Choose an assignment =="},
        {"ann", "3\n", "== Choose an assignment =="},   {"ann", "4\n", "Current password"},
    };
    f.marking.mark(f.as("lect"), f.submission(f.ann).getId(), 70, "Good", "");  // so the HoD queue is not empty
    for (const Path& p : paths) {
        INFO(p.user << " typing " << p.selections);
        CHECK(countOf(p.selections, "\n") <= 3);
        CHECK(contains(f.run(login(p.user) + p.selections), p.screen));
    }
}

TEST_CASE("AT-03: HoD, every mark in the department is seen before release and one penalty rule applies",
          "[acceptance]") {
    SystemFixture f;
    f.seed();
    f.assignments.grantExtension(f.as("lect"), f.project, f.cat, hoursAfter(200), "Bereavement");
    f.markedAt(30, f.ann, 80);           // on time
    f.markedAt(48 + 30, f.ben, 80);      // 2 days late
    f.markedAt(150, f.cat, 80);          // late by the original deadline, on time by the extension
    std::string out = f.run(login("hod") + "1\n1\n0\n0\n0\n");
    CHECK(contains(out, "Ann Student (2023001)  raw 80, penalty 0, final 80"));
    CHECK(contains(out, "Ben Student (2023002)  raw 80, penalty 20, final 60"));
    CHECK(contains(out, "Cat Student (2023003)  raw 80, penalty 0, final 80"));
    CHECK(contains(out, "Bereavement"));   // the HoD can see why Cat had no penalty (C3)
    // Nothing reaches a student until the HoD acts.
    for (int s : {f.ann, f.ben, f.cat}) CHECK(f.submission(s).getStatus() == SubmissionStatus::Marked);
}

TEST_CASE("AT-04: Lecturer, marking takes little effort and the lecturer can follow each mark", "[acceptance]") {
    SystemFixture f;
    f.seed();
    f.submitAt(48 + 5, f.ben);   // 1 day late
    // The lecturer types only the raw mark and feedback; the penalty is worked out for them.
    std::string out = f.run(login("lect") + "3\n1\n1\n1\n90\nVery good\ny\n0\n0\n0\n");
    CHECK(contains(out, "Days late: 1   Penalty: 10   Final mark: 80"));
    CHECK(contains(out, "Ben Student (2023002)  Awaiting moderation"));

    int id = f.submission(f.ben).getId();
    f.moderation.approve(f.as("hod"), id);
    CHECK(contains(f.run(login("lect") + "3\n1\n1\n0\n0\n0\n"), "Ben Student (2023002)  Moderated"));
    f.moderation.release(f.as("hod"), f.project);
    CHECK(contains(f.run(login("lect") + "3\n1\n1\n0\n0\n0\n"), "Ben Student (2023002)  Released"));
    // An extension for a good reason is two menu choices and a date.
    out = f.run(login("lect") + "2\n1\n1\n1\n" + when(hoursAfter(100)) + "\nFamily emergency\n0\n0\n");
    CHECK(contains(out, "Extension granted."));
}

TEST_CASE("AT-05: Student rep, students can fix mistakes, late work is not rejected, feedback is written",
          "[acceptance]") {
    SystemFixture f;
    f.seed();
    f.submitAt(5, f.ann);                 // the wrong file
    f.submitAt(20, f.ann);                // fixed before the deadline
    CHECK(f.submission(f.ann).getVersions().size() == 2);
    f.clock.set(hoursAfter(48 + 100));    // four days late
    std::string out = f.run(login("ben") + "2\n1\n" + f.file("ben.pdf") + "\ny\n0\n0\n");
    CHECK(contains(out, "== Receipt =="));   // accepted, never refused
    int a = f.submission(f.ann).getId(), b = f.submission(f.ben).getId();
    f.marking.mark(f.as("lect"), a, 68, "Good structure; add more tests", "");
    f.marking.mark(f.as("lect"), b, 75, "Strong work, but more than 3 days late", "");
    f.moderation.approve(f.as("hod"), a);
    f.moderation.approve(f.as("hod"), b);
    f.moderation.release(f.as("hod"), f.project);
    out = f.run(login("ben") + "3\n1\n0\n0\n");
    CHECK(contains(out, "Raw mark: 75 out of 100"));
    CHECK(contains(out, "Final mark: 0"));
    CHECK(contains(out, "Feedback: Strong work, but more than 3 days late"));
    // Privacy: Ben's screens never show Ann's work or mark.
    CHECK_FALSE(contains(out + studentList(f, "ben"), "Ann"));
    CHECK_FALSE(contains(out, "68"));
}

TEST_CASE("AT-06: IT officer, passwords are protected and the data can be backed up and restored",
          "[acceptance]") {
    SystemFixture f(true);   // real Argon2id
    f.seed();
    for (const auto& u : f.admin.listUsers(f.as("admin"), std::nullopt))
        CHECK(u->getPasswordHash().rfind("$argon2id$", 0) == 0);
    std::string guesses;
    for (int i = 0; i < 5; ++i) guesses += "1\nann\nguess" + std::to_string(i) + "\n";
    CHECK(contains(f.run(guesses + login("ann") + "0\n"), "locked"));

    f.submitAt(10, f.cat);
    std::string backup = (f.dir.path / "backup.db").string();
    CHECK(contains(f.run(login("admin") + "4\n1\n" + backup + "\n0\n0\n"), "Backup saved."));
    f.markedAt(11, f.ben, 50);
    CHECK(contains(f.run(login("admin") + "4\n2\n" + backup + "\ny\n0\n0\n"), "Database restored."));
    CHECK_FALSE(f.submissionRepo.find(f.project, f.ben));   // back to the backed-up state
    CHECK(f.submissionRepo.find(f.project, f.cat));
}

TEST_CASE("AT-07: Data protection, only the minimum personal data is kept and students see only their own",
          "[acceptance]") {
    SystemFixture f;
    f.seed();
    std::vector<std::string> columns;
    auto st = f.db.prepare("SELECT name FROM pragma_table_info('users') ORDER BY cid");
    while (st.step()) columns.push_back(st.columnText(0));
    // Name, username, number, role, plus the login fields needed for security (NFR-13).
    for (const std::string& c : columns) {
        INFO("users column " << c);
        bool allowed = false;
        for (const char* ok : {"user_id", "username", "full_name", "person_number", "role", "password_hash",
                                      "active", "failed_logins", "locked_until"})
            if (c == ok) allowed = true;
        CHECK(allowed);
    }
    f.markedAt(10, f.ann, 70);
    f.moderation.approve(f.as("hod"), f.submission(f.ann).getId());
    f.moderation.release(f.as("hod"), f.project);
    // Ben has not submitted: he cannot reach Ann's released mark in any way.
    CHECK_THROWS_WITH(f.submitting.viewReleasedMark(f.as("ben"), f.project), "You have not submitted this assignment");
    CHECK_THROWS_AS(f.marking.listSubmissions(f.as("ben"), f.project), AccessDenied);
    CHECK_THROWS_AS(f.moderation.listForModeration(f.as("ben"), f.project), AccessDenied);
    CHECK_THROWS_AS(f.admin.listUsers(f.as("ben"), std::nullopt), AccessDenied);
}
