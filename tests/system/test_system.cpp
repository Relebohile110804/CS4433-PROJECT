// System tests ST-01 to ST-44 (Week 5 test plan, Section 1.2). Each one drives the real
// program through its menus, with a real database file and real submitted files, and
// checks the result against the SRS: the use case flows of UC-09, UC-16 and UC-20, the
// functional requirements and the measurable non-functional requirements.
#include <algorithm>
#include <chrono>

#include "SystemFixture.h"
#include "business/domain/Errors.h"
#include "business/services/UserFactory.h"

using namespace systest;

namespace {

std::string login(const std::string& user) {
    return "1\n" + user + "\n" + (user == "admin" ? "admin-pass" : "password1") + "\n";
}
std::string n(int i) { return std::to_string(i) + "\n"; }

// Position of a row in a menu, counted from 1, as the user would type it.
int userRow(SystemFixture& f, const std::string& fullName, std::optional<Role> role = std::nullopt) {
    auto list = f.admin.listUsers(f.as("admin"), role);
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i]->getFullName() == fullName) return static_cast<int>(i) + 1;
    FAIL("no user " << fullName);
    return 0;
}
int courseRow(SystemFixture& f, const std::string& who, int courseId) {
    auto list = f.assignments.myCourses(f.as(who));
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].getId() == courseId) return static_cast<int>(i) + 1;
    FAIL("course not listed");
    return 0;
}
int submissionRow(SystemFixture& f, int student) {
    auto list = f.marking.listSubmissions(f.as("lect"), f.project);
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].getStudentId() == student) return static_cast<int>(i) + 1;
    FAIL("submission not listed");
    return 0;
}

std::string pathPrompt = "Full path of your file";

}  // namespace

// ---------------------------------------------------------------- UC-01 to UC-07

TEST_CASE("ST-01: FR-01 each role logs in and sees only its own menu", "[system]") {
    SystemFixture f;
    f.seed();
    std::string student = f.run(login("ann") + "0\n0\n");
    std::string lecturer = f.run(login("lect") + "0\n0\n");
    std::string hod = f.run(login("hod") + "0\n0\n");
    std::string admin = f.run(login("admin") + "0\n0\n");

    CHECK(contains(student, "Welcome, Ann Student (Student)."));
    CHECK(contains(student, "== Student =="));
    CHECK(contains(lecturer, "== Lecturer =="));
    CHECK(contains(hod, "== Head of Department =="));
    CHECK(contains(admin, "== Administrator =="));
    for (const char* other : {"Create an assignment", "Moderate marks", "Release marks", "Users", "Audit log"})
        CHECK_FALSE(contains(student, other));
    for (const char* other : {"Submit work", "Moderate marks", "Release marks", "Backup"})
        CHECK_FALSE(contains(lecturer, other));
    for (const char* other : {"Submit work", "Create an assignment", "Backup", "Users"})
        CHECK_FALSE(contains(hod, other));
    for (const char* other : {"Submit work", "Create an assignment", "Moderate marks", "Release marks"})
        CHECK_FALSE(contains(admin, other));
}

TEST_CASE("ST-02: FR-02 five failed logins lock the account for 15 minutes", "[system]") {
    SystemFixture f;
    f.seed();
    std::string wrong;
    for (int i = 0; i < 5; ++i) wrong += "1\nben\nwrong-pass\n";
    std::string out = f.run(wrong + login("ben") + "0\n");
    CHECK(countOf(out, "! Wrong username or password") == 4);
    CHECK(contains(out, "! Too many failed logins. This account is locked for 15 minutes"));
    CHECK(contains(out, "! This account is locked after too many failed logins"));
    CHECK_FALSE(contains(out, "Welcome, Ben Student"));

    f.clock.advance(std::chrono::minutes(15));
    out = f.run(login("ben") + "0\n0\n");
    CHECK(contains(out, "Welcome, Ben Student (Student)."));
}

TEST_CASE("ST-03: FR-04 the administrator creates, edits and deactivates accounts", "[system]") {
    SystemFixture f;
    f.seed();
    int catSubmission = f.submitAt(10, f.cat);
    int catRow = userRow(f, "Cat Student");
    std::string out = f.run(login("admin") +
                            "1\n1\n2\nnewlect\nNew Lecturer\nS300\npassword1\n"   // create a lecturer
                            "1\n1\n4\nann\nCopy\n2029999\npassword1\n"            // username taken
                            "1\n1\n4\nzed\nZed\n2023001\npassword1\n"             // number taken
                            "1\n3\n" + n(catRow) + "Catherine Student\n"          // edit a name
                            "0\n0\n");
    CHECK(contains(out, "User created."));
    CHECK(contains(out, "! The username ann is already taken"));
    CHECK(contains(out, "! The number 2023001 is already used"));
    CHECK(contains(out, "Name changed."));
    REQUIRE(f.users.findByUsername("newlect"));
    CHECK(f.users.findByUsername("newlect")->getRole() == Role::Lecturer);

    catRow = userRow(f, "Catherine Student");
    out = f.run(login("admin") + "1\n5\n" + n(catRow) + "y\n" + "1\n2\n0\n0\n");
    CHECK(contains(out, "Account deactivated."));
    CHECK(contains(out, "Catherine Student  2023003  cat  Student  [deactivated]"));

    out = f.run(login("cat") + "0\n");
    CHECK(contains(out, "! This account has been deactivated. Please see the administrator"));
    // Users with submissions are never deleted (FR-04): the work is still there.
    CHECK(f.submissionRepo.findById(catSubmission));
    CHECK(f.users.findByUsername("cat"));
}

TEST_CASE("ST-04: FR-03 a user changes their own password", "[system]") {
    SystemFixture f;
    f.seed();
    std::string out = f.run(login("ann") +
                            "4\npassword1\nshort7c\nshort7c\n"          // 7 characters
                            "4\npassword1\nnewpass12\nnewpass13\n"      // typed differently
                            "4\nwrong-pass\nnewpass12\nnewpass12\n"     // wrong current password
                            "4\npassword1\nnewpass12\nnewpass12\n"      // correct
                            "0\n"
                            "1\nann\npassword1\n"                       // the old one no longer works
                            "1\nann\nnewpass12\n0\n0\n");
    CHECK(contains(out, "! The password must be at least 8 characters"));
    CHECK(contains(out, "! The two new passwords are different. Nothing was changed."));
    CHECK(contains(out, "! The current password is wrong"));
    CHECK(contains(out, "Password changed."));
    CHECK(contains(out, "! Wrong username or password"));
    CHECK(countOf(out, "Welcome, Ann Student") == 2);
}

TEST_CASE("ST-05: FR-05 departments and courses are created, edited and listed", "[system]") {
    SystemFixture f;
    f.seed();
    Session s = f.as("admin");
    f.admin.createUser(s, Role::HeadOfDepartment, "hod3", "Ms Physics", "S102", "password1");
    int newHod = userRow(f, "Ms Physics", Role::HeadOfDepartment);
    int csHod = userRow(f, "Mrs Head", Role::HeadOfDepartment);
    std::string out = f.run(login("admin") + "2\n1\nPhysics\n" + n(newHod) +   // new department
                            "2\n1\nChemistry\n" + n(csHod) +                    // HoD already heads one
                            "0\n0\n");
    CHECK(contains(out, "Department created."));
    CHECK(contains(out, "! This Head of Department already heads Computer Science"));

    // Departments are listed by name: Computer Science, Mathematics, Physics.
    out = f.run(login("admin") + "2\n3\n3\nPH1000\nMechanics\n"            // course in Physics
                "2\n3\n1\nCS4433\nCopy\n"                                  // code already used
                "2\n4\n" + n(courseRow(f, "admin", f.cs4433)) + "CS4433\nSoftware Engineering II\n"
                "2\n5\n0\n0\n");
    CHECK(contains(out, "Course created."));
    CHECK(contains(out, "! A course with code CS4433 already exists"));
    CHECK(contains(out, "Course changed."));
    CHECK(contains(out, "== Physics, HoD: Ms Physics (S102) =="));
    CHECK(contains(out, "PH1000  Mechanics  (0 lecturers, 0 students)"));
    CHECK(contains(out, "CS4433  Software Engineering II  (1 lecturers, 3 students)"));
}

TEST_CASE("ST-06: FR-06, FR-07 lecturers are assigned and removed, students enrolled and withdrawn", "[system]") {
    SystemFixture f;
    f.seed();
    int cs = courseRow(f, "admin", f.cs4433), ma = courseRow(f, "admin", f.ma1000);
    int numbers = userRow(f, "Ms Numbers", Role::Lecturer);
    int benRow = userRow(f, "Ben Student", Role::Student), annRow = userRow(f, "Ann Student", Role::Student);
    std::string out = f.run(login("admin") +
                            "3\n1\n" + n(cs) + n(numbers) +     // assign a second lecturer
                            "3\n1\n" + n(cs) + n(numbers) +     // already teaches it
                            "3\n2\n" + n(cs) + n(numbers) +     // remove again
                            "3\n3\n" + n(ma) + n(benRow) +      // enrol
                            "3\n3\n" + n(cs) + n(annRow) +      // already enrolled
                            "0\n0\n");
    CHECK(contains(out, "Lecturer assigned."));
    CHECK(contains(out, "! Ms Numbers already teaches CS4433"));
    CHECK(contains(out, "Lecturer removed."));
    CHECK(contains(out, "Student enrolled."));
    CHECK(contains(out, "! Ann Student is already enrolled in CS4433"));
    CHECK_FALSE(f.courses.isTeaching(f.lect2, f.cs4433));
    CHECK(f.assignments.listForStudent(f.as("ben")).size() == 2);

    out = f.run(login("admin") + "3\n4\n" + n(ma) + n(benRow) + "0\n0\n");
    CHECK(contains(out, "Student withdrawn."));
    CHECK(f.assignments.listForStudent(f.as("ben")).size() == 1);
}

TEST_CASE("ST-07: FR-08 a lecturer creates an assignment through the menus", "[system]") {
    SystemFixture f;
    f.seed();
    std::string out = f.run(login("lect") +
                            "1\n1\nEssay\nShort essay\n2020-01-01 10:00\n50\n"      // deadline in the past
                            "1\n1\nEssay\nShort essay\n2027-02-29 10:00\n2026-12-01 17:00\n0\n1001\n50\n"
                            "0\n0\n");
    CHECK(contains(out, "! The deadline must be in the future"));
    CHECK(contains(out, "! Please type a real date and time, for example 2026-11-07 17:00."));
    CHECK(countOf(out, "! Please type a whole number from 1 to 1000.") == 2);
    CHECK(contains(out, "Assignment created."));

    out = f.run(login("ann") + "1\n0\n0\n");
    CHECK(contains(out, "Essay, out of 50"));
    CHECK(contains(out, "Due 2026-12-01 17:00"));
}

TEST_CASE("ST-08: FR-09 a lecturer grants one student an extension", "[system]") {
    SystemFixture f;
    f.seed();
    // Students are listed in the order they were created: Ann, Ben, Cat.
    std::string out = f.run(login("lect") + "2\n1\n1\n1\n" + when(hoursAfter(24)) + "\nIll\n"   // earlier
                            "2\n1\n1\n1\n" + when(hoursAfter(96)) + "\n\nIllness, doctor's note\n"   // no reason first
                            "0\n0\n");
    CHECK(contains(out, "! The new deadline must be later than the student's current deadline"));
    CHECK(contains(out, "! Please type something."));
    CHECK(contains(out, "Extension granted."));
    CHECK(f.assignments.effectiveDeadline(f.project, f.ann) == hoursAfter(96));
    CHECK(f.assignments.effectiveDeadline(f.project, f.ben) == f.due);

    out = f.run(login("ann") + "1\n0\n0\n");
    CHECK(contains(out, "Due " + when(hoursAfter(96)) + " (extended)"));
    // Ann hands in after the original deadline but before her own: on time.
    f.submitAt(90, f.ann);
    CHECK(f.submission(f.ann).getStatus() == SubmissionStatus::Submitted);
}

TEST_CASE("ST-09: FR-10 the student list shows each course, deadline and status", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    std::string out = f.run(login("ann") + "1\n0\n0\n");
    CHECK(contains(out, "My assignments (2)"));
    CHECK(contains(out, "CS4433  Project, out of 100"));
    CHECK(contains(out, "MA1000  Homework, out of 50"));
    CHECK(contains(out, "Due " + when(f.due) + "   Status: Submitted"));
    CHECK(contains(out, "Due " + when(hoursAfter(72)) + "   Status: Not submitted"));
}

// ---------------------------------------------------------------- UC-09 Submit Assignment

TEST_CASE("ST-10: UC-09 main flow, an on-time submission with a receipt", "[system]") {
    SystemFixture f;
    f.seed();
    f.clock.set(hoursAfter(10));
    std::string path = f.file("report.pdf", 5000);
    std::string out = f.run(login("ann") + "2\n1\n" + path + "\ny\n0\n0\n");

    CHECK(contains(out, "Your deadline: " + when(f.due)));                // step 2
    CHECK(contains(out, "The submission is on time."));                   // step 4
    CHECK(contains(out, "== Receipt =="));                                // step 6
    CHECK(contains(out, "Assignment: Project"));
    CHECK(contains(out, "Version 1, handed in " + when(hoursAfter(10))));
    CHECK(contains(out, "Status: Submitted"));
    CHECK(contains(out, "File check (SHA-256): " + f.files.sha256(path).substr(0, 8)));

    Submission sub = f.submission(f.ann);
    CHECK(sub.getStatus() == SubmissionStatus::Submitted);
    const SubmissionVersion& v = sub.currentVersion();
    CHECK(fs::exists(v.getStoredPath()));
    CHECK(v.getSha256() == f.files.sha256(path));
    CHECK(v.getSubmittedAt() == hoursAfter(10));
}

TEST_CASE("ST-11: UC-09 A1, resubmitting before the deadline warns and keeps the old version", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(5, f.ann);
    f.clock.set(hoursAfter(20));
    std::string out = f.run(login("ann") + "2\n1\n" + f.file("report-v2.pdf") + "\ny\n0\n0\n");
    CHECK(contains(out, "You have already handed in version 1."));
    CHECK(contains(out, "will replace it"));
    CHECK(contains(out, "Version 2, handed in " + when(hoursAfter(20))));
    CHECK(contains(out, "Status: Submitted"));
    Submission sub = f.submission(f.ann);
    CHECK(sub.getVersions().size() == 2);
    CHECK(sub.getStatus() == SubmissionStatus::Submitted);
    CHECK(f.storedFiles() == 2);
}

TEST_CASE("ST-12: UC-09 A2, a submission 1 to 3 days late shows the penalty first", "[system]") {
    SystemFixture f;
    f.seed();
    f.clock.set(hoursAfter(50));  // 2 hours late: 1 day
    std::string out = f.run(login("ben") + "2\n1\n" + f.file("ben.docx") + "\ny\n0\n0\n");
    CHECK(contains(out, "This submission is 1 day late: 10% of the maximum mark will be deducted."));
    CHECK(contains(out, "Status: Late"));
    CHECK(f.submission(f.ben).getStatus() == SubmissionStatus::Late);
    CHECK(f.submission(f.ben).getDaysLate() == 1);

    f.clock.set(hoursAfter(78));  // 30 hours late: 2 days
    out = f.run(login("cat") + "2\n1\n" + f.file("cat.zip") + "\ny\n0\n0\n");
    CHECK(contains(out, "This submission is 2 days late: 20% of the maximum mark will be deducted."));
    CHECK(f.submission(f.cat).getDaysLate() == 2);
}

TEST_CASE("ST-13: UC-09 A2, a submission more than 3 days late is still accepted", "[system]") {
    SystemFixture f;
    f.seed();
    f.clock.set(hoursAfter(48 + 73));
    std::string out = f.run(login("ben") + "2\n1\n" + f.file("ben.pdf") + "\ny\n0\n0\n");
    CHECK(contains(out, "This submission is more than 3 days late: the mark will be 0."));
    CHECK(contains(out, "== Receipt =="));
    CHECK(f.submission(f.ben).getDaysLate() == 4);
}

TEST_CASE("ST-14: UC-09 A3, the student cancels and nothing is stored", "[system]") {
    SystemFixture f;
    f.seed();
    f.clock.set(hoursAfter(10));
    std::string out = f.run(login("ann") + "2\n1\n" + f.file("report.pdf") + "\nn\n0\n0\n");
    CHECK(contains(out, "Nothing was submitted."));
    CHECK_FALSE(contains(out, "== Receipt =="));
    CHECK_FALSE(f.submissionRepo.find(f.project, f.ann));
    CHECK(f.storedFiles() == 0);
}

TEST_CASE("ST-15: UC-09 E1, three wrong files return the student to the menu", "[system]") {
    SystemFixture f;
    f.seed();
    f.clock.set(hoursAfter(10));
    std::string missing = (f.dir.path / "work" / "missing.pdf").string();
    std::string out = f.run(login("ann") + "2\n1\n" + missing + "\n" + f.file("photo.jpg") + "\n" +
                            f.file("big.pdf", 10485761) + "\n0\n0\n");
    // The path is long, so the message is wrapped onto a second line (NFR-11).
    CHECK(contains(out, "! The file cannot be found or opened:"));
    CHECK(contains(out, "missing.pdf"));
    CHECK(contains(out, "! Only .pdf, .docx, .txt, .zip, .cpp and .h files can be submitted"));
    CHECK(contains(out, "! The file is larger than 10 MB"));
    CHECK(countOf(out, pathPrompt) == 3);
    CHECK(contains(out, "Three attempts failed. Returning to the menu."));
    CHECK_FALSE(contains(out, "Submit this file"));
    CHECK(f.storedFiles() == 0);
}

TEST_CASE("ST-16: UC-09 E1, a wrong file and then the right one is accepted", "[system]") {
    SystemFixture f;
    f.seed();
    f.clock.set(hoursAfter(10));
    std::string out = f.run(login("ann") + "2\n1\n" + f.file("notes.doc") + "\n" + f.file("main.cpp") +
                            "\ny\n0\n0\n");
    CHECK(contains(out, "! Only .pdf, .docx, .txt, .zip, .cpp and .h files can be submitted"));
    CHECK(contains(out, "== Receipt =="));
    CHECK(f.submissionRepo.find(f.project, f.ann));
}

TEST_CASE("ST-17: UC-09 E2, after the deadline the version already submitted stands", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);   // on time
    f.submitAt(50, f.ben);   // late
    f.clock.set(hoursAfter(60));
    std::string ann = f.run(login("ann") + "2\n1\n0\n0\n");
    std::string ben = f.run(login("ben") + "2\n1\n0\n0\n");
    CHECK(contains(ann, "! The deadline has passed, so the version already submitted stands (BR-02)"));
    CHECK(contains(ben, "! A late submission cannot be replaced (BR-02)"));
    // Refused at step 2, before the student is asked for a file.
    CHECK_FALSE(contains(ann, pathPrompt));
    CHECK_FALSE(contains(ben, pathPrompt));
    CHECK(f.submission(f.ann).getVersions().size() == 1);
}

TEST_CASE("ST-18: UC-09 E3, marked work cannot be submitted again", "[system]") {
    SystemFixture f;
    f.seed();
    f.markedAt(10, f.ann, 70);
    f.clock.set(hoursAfter(20));  // still before the deadline
    std::string out = f.run(login("ann") + "2\n1\n0\n0\n");
    CHECK(contains(out, "has already been marked"));
    CHECK_FALSE(contains(out, pathPrompt));
}

TEST_CASE("ST-19: UC-09 E4, a file store error leaves nothing stored and says to try again", "[system]") {
    SystemFixture f;
    f.seed();
    f.clock.set(hoursAfter(10));
    // A file already sits where the copy would go, so the copy fails.
    fs::path blocker = f.dir.path / "submissions" /
                       ("a" + std::to_string(f.project) + "_s" + std::to_string(f.ann) + "_v1.pdf");
    std::ofstream(blocker) << "not the student's work";
    std::string out = f.run(login("ann") + "2\n1\n" + f.file("report.pdf") + "\ny\n0\n0\n");
    CHECK(contains(out, "The work was not submitted because of an error"));
    CHECK(contains(out, "Please try again."));
    CHECK_FALSE(f.submissionRepo.find(f.project, f.ann));
    CHECK(f.storedFiles() == 1);
    CHECK(contains(f.errorLogText(), "already exists"));
    CHECK(countOf(out, "== Student ==") >= 2);  // back at the menu
}

// ---------------------------------------------------------------- UC-16 Mark Submission

TEST_CASE("ST-20: UC-16 main flow, list, mark, penalty and audit entry", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    f.submitAt(50, f.ben);
    std::string stored = f.submission(f.ann).currentVersion().getStoredPath();
    std::string out = f.run(login("lect") + "3\n1\n1\n" + n(submissionRow(f, f.ann)) +
                            "75\nWell structured\ny\n" + n(submissionRow(f, f.ben)) + "80\nStrong but late\ny\n" +
                            "0\n0\n0\n");
    // Step 1: student, time received, status and days late (FR-14).
    CHECK(contains(out, "Ann Student (2023001)"));
    CHECK(contains(out, "     handed in " + when(hoursAfter(10))));
    CHECK(contains(out, "     handed in " + when(hoursAfter(50))));
    CHECK(contains(out, "Late, 1 day(s) late"));
    CHECK(contains(unwrapped(out), stored));                                   // step 2: where the file is
    CHECK(contains(out, "Days late: 0   Penalty: 0   Final mark: 75"));        // step 5
    CHECK(contains(out, "Days late: 1   Penalty: 10   Final mark: 70"));
    CHECK(countOf(out, "Mark saved. It now waits for the Head of Department.") == 2);  // step 7

    Submission sub = f.submission(f.ann);
    CHECK(sub.getStatus() == SubmissionStatus::Marked);
    CHECK(sub.getMark()->getMarkedBy() == f.lect);
    auto log = f.auditRepo.query(std::nullopt, std::nullopt, std::nullopt);
    REQUIRE(log.size() == 2);
    CHECK_FALSE(log[0].getOldValue());
    CHECK(log[0].getNewValue() == 75);
    CHECK(log[0].getChangedBy() == f.lect);
    CHECK(log[0].getReason() == "Initial marking");  // step 6
    CHECK_FALSE(f.submitting.findOwn(f.as("ann"), f.project)->getMark());  // still hidden
}

TEST_CASE("ST-21: UC-16 E1, E2, a wrong mark or empty feedback is asked for again", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    std::string out = f.run(login("lect") + "3\n1\n1\n1\nabc\n101\n-1\n7.5\n75\n\nWell structured\ny\n0\n0\n0\n");
    CHECK(countOf(out, "! Please type a whole number from 0 to 100.") == 4);
    CHECK(contains(out, "! Please type something."));
    CHECK(contains(out, "Mark saved."));
    CHECK(f.submission(f.ann).getMark()->getRawMark() == 75);
}

TEST_CASE("ST-22: UC-16 A1, a returned submission is re-marked with a reason", "[system]") {
    SystemFixture f;
    f.seed();
    int id = f.markedAt(10, f.ann, 60);
    f.moderation.returnForRemarking(f.as("hod"), id, "Check question 3 again");
    std::string out = f.run(login("lect") + "3\n1\n1\n1\n70\nQuestion 3 re-marked\nQuestion 3 was right\ny\n0\n0\n0\n");
    CHECK(contains(out, "Returned for re-marking"));
    CHECK(contains(out, "HoD comment: Check question 3 again"));
    CHECK(contains(out, "Reason for changing the mark"));
    CHECK(contains(out, "Mark saved."));
    CHECK(f.submission(f.ann).getStatus() == SubmissionStatus::Marked);
    auto log = f.auditRepo.query(std::nullopt, std::nullopt, std::nullopt);
    REQUIRE(log.size() == 2);
    CHECK(*log[1].getOldValue() == 60);
    CHECK(log[1].getNewValue() == 70);
    CHECK(log[1].getReason() == "Question 3 was right");
}

TEST_CASE("ST-23: UC-16 A2, the lecturer corrects their own mark before moderation", "[system]") {
    SystemFixture f;
    f.seed();
    f.markedAt(10, f.ann, 60);
    std::string out = f.run(login("lect") + "3\n1\n1\n1\n65\nAdded a missed answer\n\nMissed page 4\ny\n0\n0\n0\n");
    CHECK(contains(out, "Current final mark: 60"));
    CHECK(contains(out, "! Please type something."));   // the reason cannot be empty
    CHECK(contains(out, "Mark saved."));
    auto log = f.auditRepo.query(std::nullopt, std::nullopt, std::nullopt);
    REQUIRE(log.size() == 2);
    CHECK(*log[1].getOldValue() == 60);
    CHECK(log[1].getNewValue() == 65);
}

TEST_CASE("ST-24: UC-16 A3, the lecturer cancels and nothing is saved", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    std::string out = f.run(login("lect") + "3\n1\n1\n1\n75\nGood\nn\n0\n0\n0\n");
    CHECK(contains(out, "Nothing was saved."));
    CHECK(f.submission(f.ann).getStatus() == SubmissionStatus::Submitted);
    CHECK(f.auditRepo.query(std::nullopt, std::nullopt, std::nullopt).empty());
}

TEST_CASE("ST-25: UC-16 E3, a moderated mark is refused and the lecturer is sent to the HoD", "[system]") {
    SystemFixture f;
    f.seed();
    int id = f.markedAt(10, f.ann, 60);
    f.moderation.approve(f.as("hod"), id);
    std::string out = f.run(login("lect") + "3\n1\n1\n1\n0\n0\n0\n");
    CHECK(contains(out, "Please contact the Head of Department"));
    CHECK_FALSE(contains(out, "Raw mark (0-100)"));
    CHECK(f.submission(f.ann).getStatus() == SubmissionStatus::Moderated);
}

TEST_CASE("ST-26: FR-22 the lecturer sees every moderation status", "[system]") {
    SystemFixture f;
    f.seed();
    int dan = f.admin.createUser(f.as("admin"), Role::Student, "dan", "Dan Student", "2023004", "password1");
    f.admin.enrolStudent(f.as("admin"), f.cs4433, dan);
    int a = f.markedAt(10, f.ann, 70);
    f.moderation.approve(f.as("hod"), a);
    f.moderation.release(f.as("hod"), f.project);
    f.moderation.approve(f.as("hod"), f.markedAt(11, f.ben, 60));
    f.moderation.returnForRemarking(f.as("hod"), f.markedAt(12, f.cat, 50), "Too low?");
    f.clock.set(hoursAfter(13));
    f.submitting.submit(f.as("dan"), f.project, f.file("dan.pdf"));
    f.marking.mark(f.as("lect"), f.submission(dan).getId(), 55, "Fine", "");

    std::string out = f.run(login("lect") + "3\n1\n1\n0\n0\n0\n");
    CHECK(contains(out, "Ann Student (2023001)  Released"));
    CHECK(contains(out, "Ben Student (2023002)  Moderated"));
    CHECK(contains(out, "Cat Student (2023003)  Returned for re-marking"));
    CHECK(contains(out, "Dan Student (2023004)  Awaiting moderation"));
}

// ---------------------------------------------------------------- UC-20 Moderate and Release Marks

TEST_CASE("ST-27: UC-20 step 1, the queue counts the submissions in each status", "[system]") {
    SystemFixture f;
    f.seed();
    f.markedAt(10, f.ann, 70);
    f.moderation.approve(f.as("hod"), f.markedAt(11, f.ben, 60));
    f.submitAt(12, f.cat);
    std::string out = f.run(login("hod") + "1\n0\n0\n0\n");
    CHECK(contains(out, "Project (CS4433)"));
    CHECK(contains(out, "1 awaiting moderation"));
    CHECK(contains(out, "1 moderated"));
}

TEST_CASE("ST-28: UC-20 step 2, the list shows feedback, marker and any extension", "[system]") {
    SystemFixture f;
    f.seed();
    f.assignments.grantExtension(f.as("lect"), f.project, f.ben, hoursAfter(96), "Illness");
    f.markedAt(10, f.ann, 70, "Clear and complete");
    f.markedAt(60, f.ben, 65, "Good testing");
    std::string out = f.run(login("hod") + "1\n1\n0\n0\n0\n0\n");
    CHECK(contains(out, "raw 70, penalty 0, final 70"));
    CHECK(contains(out, "Clear and complete"));
    CHECK(contains(out, "Good testing"));
    CHECK(contains(out, "Mr Teacher"));                // the marker
    CHECK(contains(out, when(hoursAfter(96))));        // Ben's extension
    CHECK(contains(out, "Illness"));
}

TEST_CASE("ST-29: UC-20 step 3 and A1, approve one mark and adjust another", "[system]") {
    SystemFixture f;
    f.seed();
    int a = f.markedAt(10, f.ann, 70);
    int b = f.markedAt(11, f.ben, 60);
    f.clock.set(hoursAfter(100));
    std::string out = f.run(login("hod") + "1\n1\n1\n1\n" +                 // approve Ann
                            "2\n2\n101\n-1\n65\n\nAgreed with the lecturer\n" // adjust Ben (E1 first)
                            "0\n0\n0\n");
    CHECK(contains(out, "Mark approved."));
    CHECK(countOf(out, "! Please type a number from 0 to 100.") == 2);
    CHECK(contains(out, "! Please type something."));
    CHECK(contains(out, "Mark adjusted."));
    Submission ann = *f.submissionRepo.findById(a), ben = *f.submissionRepo.findById(b);
    CHECK(ann.getStatus() == SubmissionStatus::Moderated);
    CHECK(*ann.getModeratedBy() == f.hod);
    CHECK(*ann.getModeratedAt() == hoursAfter(100));
    CHECK(ben.getStatus() == SubmissionStatus::Moderated);
    CHECK(ben.getMark()->getFinalMark() == 65);
    auto log = f.auditRepo.query(std::nullopt, std::nullopt, std::nullopt);
    REQUIRE(log.size() == 3);
    CHECK(*log[2].getOldValue() == 60);
    CHECK(log[2].getNewValue() == 65);
    CHECK(log[2].getChangedBy() == f.hod);
    CHECK(log[2].getReason() == "Agreed with the lecturer");
}

TEST_CASE("ST-30: UC-20 A2, the HoD returns a mark to the lecturer", "[system]") {
    SystemFixture f;
    f.seed();
    int id = f.markedAt(10, f.ann, 70);
    std::string out = f.run(login("hod") + "1\n1\n1\n3\nPlease check question 3\n0\n0\n0\n");
    CHECK(contains(out, "Returned to the lecturer."));
    Submission sub = *f.submissionRepo.findById(id);
    CHECK(sub.getStatus() == SubmissionStatus::Returned);
    CHECK(sub.getHodComment() == "Please check question 3");
    out = f.run(login("lect") + "3\n1\n1\n0\n0\n0\n");
    CHECK(contains(out, "Ann Student (2023001)  Returned for re-marking"));
}

TEST_CASE("ST-31: UC-20 steps 4 to 6, release shows the counts and only moderated marks go out", "[system]") {
    SystemFixture f;
    f.seed();
    f.moderation.approve(f.as("hod"), f.markedAt(10, f.ann, 70, "Clear and complete"));
    f.markedAt(11, f.ben, 60);
    f.submitAt(12, f.cat);
    std::string out = f.run(login("hod") + "2\n1\n1\ny\n0\n0\n");
    CHECK(contains(out, "1 moderated mark(s) will be released."));
    CHECK(contains(out, "1 marked submission(s) are not yet moderated and will stay hidden."));
    CHECK(contains(out, "1 mark(s) released."));
    CHECK(f.submission(f.ann).getStatus() == SubmissionStatus::Released);
    CHECK(f.submission(f.ben).getStatus() == SubmissionStatus::Marked);

    out = f.run(login("ann") + "3\n1\n0\n0\n");   // FR-18
    CHECK(contains(out, "Raw mark: 70 out of 100"));
    CHECK(contains(out, "Late penalty: 0"));
    CHECK(contains(out, "Final mark: 70"));
    CHECK(contains(out, "Feedback: Clear and complete"));
    out = f.run(login("ben") + "3\n1\n0\n0\n");   // FR-17: still hidden
    CHECK(contains(out, "! Your mark has not been released yet"));
}

TEST_CASE("ST-32: UC-20 A3, nothing moderated means nothing to release", "[system]") {
    SystemFixture f;
    f.seed();
    f.markedAt(10, f.ann, 70);
    std::string out = f.run(login("hod") + "2\n1\n1\n0\n0\n");
    CHECK(contains(out, "! There are no moderated marks to release for this assignment"));
    CHECK_FALSE(contains(out, "(y/n)"));  // reported before asking to confirm
}

TEST_CASE("ST-33: UC-20 A4, approvals are kept when the HoD leaves before releasing", "[system]") {
    SystemFixture f;
    f.seed();
    int id = f.markedAt(10, f.ann, 70);
    f.run(login("hod") + "1\n1\n1\n1\n0\n0\n0\n0\n");
    CHECK(f.submissionRepo.findById(id)->getStatus() == SubmissionStatus::Moderated);
    CHECK_THROWS_AS(f.submitting.viewReleasedMark(f.as("ann"), f.project), RuleViolation);
}

TEST_CASE("ST-34: BR-05 an HoD sees nothing from another department", "[system]") {
    SystemFixture f;
    f.seed();
    int id = f.markedAt(10, f.ann, 70);
    std::string out = f.run(login("hod2") + "1\n2\n0\n3\n1\n0\n0\n");
    CHECK(contains(out, "There are no assignments here."));
    CHECK(contains(out, "MA1000"));
    CHECK_FALSE(contains(out, "CS4433"));
    CHECK(contains(out, "Audit log (0 entries)"));
    CHECK_THROWS_AS(f.moderation.approve(f.as("hod2"), id), AccessDenied);
}

TEST_CASE("ST-35: BR-06 after release the HoD can still correct a mark with a reason", "[system]") {
    SystemFixture f;
    f.seed();
    int id = f.markedAt(10, f.ann, 70);
    f.moderation.approve(f.as("hod"), id);
    f.moderation.release(f.as("hod"), f.project);
    std::string out = f.run(login("hod") + "1\n1\n1\n2\n65\nAdding error found after release\n0\n0\n0\n");
    CHECK(contains(out, "Mark adjusted."));
    Submission sub = *f.submissionRepo.findById(id);
    CHECK(sub.getStatus() == SubmissionStatus::Released);
    CHECK(sub.getMark()->getFinalMark() == 65);
    CHECK(f.auditRepo.query(std::nullopt, std::nullopt, std::nullopt).size() == 2);
}

// ---------------------------------------------------------------- FR-19, FR-20, FR-23

TEST_CASE("ST-36: FR-19, FR-20 the audit log for the school, a department, a course and an assignment", "[system]") {
    SystemFixture f;
    f.seed();
    int quiz = f.assignments.createAssignment(f.as("lect"), f.cs4433, "Quiz", "", hoursAfter(30), 20);
    f.markedAt(10, f.ann, 70);
    int b = f.markedAt(11, f.ben, 60);
    f.clock.set(hoursAfter(12));
    f.marking.mark(f.as("lect"), b, 62, "Good work", "Missed a page");
    f.submitting.submit(f.as("cat"), quiz, f.file("quiz.txt"));
    f.marking.mark(f.as("lect"), f.submission(f.cat, quiz).getId(), 15, "Fine", "");
    f.submitting.submit(f.as("ann"), f.homework, f.file("homework.pdf"));
    f.marking.mark(f.as("lect2"), f.submission(f.ann, f.homework).getId(), 40, "Neat", "");

    std::string admin = f.run(login("admin") + "5\n1\n" + "5\n2\n" + n(courseRow(f, "admin", f.ma1000)) + "0\n0\n");
    CHECK(contains(admin, "Audit log (5 entries)"));
    CHECK(contains(admin, "Audit log (1 entries)"));
    // Assignments in CS4433 are listed by deadline: Quiz, then Project.
    std::string hod = f.run(login("hod") + "3\n1\n" + "3\n2\n1\n" + "3\n3\n1\n2\n" + "0\n0\n");
    CHECK(countOf(hod, "Audit log (4 entries)") == 2);
    CHECK(contains(hod, "Audit log (3 entries)"));
    CHECK(contains(hod, "- -> 70  by Mr Teacher (S200)"));
    CHECK(contains(hod, "60 -> 62  by Mr Teacher (S200)"));
    CHECK(contains(hod, "Reason: Missed a page"));
    CHECK_FALSE(contains(hod, "Ms Numbers"));  // the Mathematics entry is not shown
}

TEST_CASE("ST-37: FR-23 the administrator backs up and restores the database", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    std::string backup = (f.dir.path / "backup-2026-11-03.db").string();
    std::string out = f.run(login("admin") + "4\n1\n" + backup + "\n0\n0\n");
    CHECK(contains(out, "Backup saved."));
    CHECK(fs::exists(backup));

    f.admin.createUser(f.as("admin"), Role::Student, "newbie", "New Student", "2023099", "password1");
    out = f.run(login("admin") + "4\n2\n" + f.file("report.pdf") + "\ny\n" +   // not a backup
                "4\n2\n" + backup + "\ny\n0\n0\n");
    CHECK(contains(out, "is not a Moderated LMS backup"));
    CHECK(contains(out, "Database restored. Please log out and log in again."));
    CHECK_FALSE(f.users.findByUsername("newbie"));
    CHECK(f.users.findByUsername("ann"));
    CHECK(f.submissionRepo.find(f.project, f.ann));
}

// ---------------------------------------------------------------- non-functional requirements

TEST_CASE("ST-38: NFR-09 more than 50 wrong inputs, each answered with a message, no crash", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    std::string backup = (f.dir.path / "nfr09.db").string();
    std::string ma = n(courseRow(f, "admin", f.ma1000)), csAdmin = n(courseRow(f, "admin", f.cs4433));
    int runs = 0;
    auto session = [&](const std::string& input) {
        ++runs;
        return f.run(input);
    };
    session("9\nx\n-1\n\n"                                     // not menu choices
            "1\nnobody\nwrong-pass\n"                          // unknown user
            "1\n\nadmin\nwrong-pass\n"                         // empty username, wrong password
            "0\n");
    session(login("admin") + "7\nabc\n"
            "1\n1\n4\nann\nX\n2099001\npassword1\n"            // username taken
            "1\n1\n4\nzed\nZed\n2023001\npassword1\n"          // number taken
            "1\n1\n4\nzed\nZed\n2099002\nshort\n"              // password too short
            "1\n1\n9\n0\n"                                     // no such role
            "1\n5\n" + n(userRow(f, "System Admin")) + "y\n"   // deactivate yourself
            "2\n1\nChemistry\n" + n(userRow(f, "Mrs Head", Role::HeadOfDepartment)) +
            "2\n3\n1\nCS4433\nCopy\n"                          // course code taken
            "3\n3\n" + csAdmin + n(userRow(f, "Ann Student", Role::Student)) +    // already enrolled
            "3\n4\n" + ma + n(userRow(f, "Ben Student", Role::Student)) +         // not enrolled
            "3\n2\n" + ma + n(userRow(f, "Mr Teacher", Role::Lecturer)) +         // does not teach it
            "4\n2\nno-such-file.db\nmaybe\ny\n"                // bad confirm, then missing backup
            "4\n1\n\n" + backup + "\n"                         // empty file name first
            "5\n4\n0\n"
            "6\nadmin-pass\nnewpass12\nnewpass99\n"
            "6\nwrong-pass\nnewpass12\nnewpass12\n"
            "0\n0\n");
    session(login("lect") +
            "1\n1\nEssay\n\n2020-01-01 10:00\n50\n"
            "1\n1\nEssay\n\n2026-13-01 10:00\n2027-02-29 10:00\n7/11/2026\n2026-12-01 17:00\n0\n1001\nx\n50\n"
            "2\n1\n1\n1\n" + when(hoursAfter(24)) + "\nReason\n"
            "0\n0\n");
    // Project is due first, so it is assignment 1 of CS4433.
    session(login("lect") + "3\n1\n1\n1\nabc\n101\n-1\n75\n\nGood\ny\n0\n0\n0\n");
    session(login("ann") + "5\nq\n" + "3\n2\n" + "3\n1\n" + "0\n0\n");
    session(login("hod") + "9\n1\n1\n1\n2\n101\nabc\n-1\n65\n\nReason\n0\n0\n0\n");
    std::string locked;
    for (int i = 0; i < 5; ++i) locked += "1\nben\nwrong-pass\n";
    session(locked + login("ben") + "0\n");

    int messages = countOf("\n" + f.allOutput, "\n! ");
    WARN("NFR-09: " << messages << " wrong inputs or refused actions, each answered with a message");
    CHECK(messages >= 50);
    CHECK(countOf(f.allOutput, "Goodbye.") == runs);  // every session ended normally
}

TEST_CASE("ST-39: NFR-01 login takes under 2 seconds with Argon2id and 860 users stored", "[system][performance]") {
    SystemFixture f(true);
    // One real Argon2id hash shared by every seeded account, so seeding stays quick;
    // each login still runs a full Argon2id verification.
    std::string hash = f.realHasher.hash("password1");
    {
        auto t = f.db.begin();
        auto add = [&](Role role, const std::string& name) {
            auto u = services::UserFactory::create(role, 0, name, name, name);
            u->setPasswordHash(hash);
            f.users.add(*u);
        };
        for (int i = 0; i < 60; ++i) add(i == 0 ? Role::Administrator : Role::Lecturer, "staff" + std::to_string(i));
        for (int i = 0; i < 800; ++i) add(Role::Student, "student" + std::to_string(i));
        t->commit();
    }
    std::vector<double> ms;
    for (int i = 0; i < 20; ++i) {
        auto start = std::chrono::steady_clock::now();
        f.auth.login("student" + std::to_string(i * 37), "password1");
        ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }
    std::sort(ms.begin(), ms.end());
    double p95 = ms[18];  // the 19th of 20 sorted times
    WARN("NFR-01: login 95th percentile " << p95 << " ms over 20 logins");
    CHECK(p95 < 2000);
}

TEST_CASE("ST-40: NFR-04 no plain-text password is stored anywhere", "[system]") {
    SystemFixture f(true);
    std::string out = f.run("admin\nSystem Admin\nS001\nFirstAdminPw1\n"
                            "1\nadmin\nFirstAdminPw1\n1\n1\n4\nann\nAnn Student\n2023001\nAnnSecret-77\n"
                            "6\nFirstAdminPw1\nChangedAdminPw2\nChangedAdminPw2\n0\n0\n");
    REQUIRE(contains(out, "Password changed."));
    for (const auto& u : f.admin.listUsers(f.auth.login("admin", "ChangedAdminPw2"), std::nullopt))
        CHECK(u->getPasswordHash().rfind("$argon2id$", 0) == 0);

    std::string everything;
    for (const auto& e : fs::recursive_directory_iterator(f.dir.path)) {
        if (!e.is_regular_file()) continue;
        std::ifstream in(e.path(), std::ios::binary);
        everything += std::string(std::istreambuf_iterator<char>(in), {});
    }
    for (std::string pw : {"FirstAdminPw1", "AnnSecret-77", "ChangedAdminPw2"}) {
        CHECK_FALSE(contains(everything, pw));
        CHECK(countOf(out, pw) == countOf(out, pw + "\n"));  // only where the user typed it
    }
}

TEST_CASE("ST-41: NFR-11 every screen line fits in 80 columns and uses no colour codes", "[system]") {
    SystemFixture f;
    std::string longName = "Mamosa Elizabeth Ntsoaki Mokhothu-Ramakatsa Lebona";  // 50 characters
    std::string longTitle = "Software Engineering Project: Moderated Learning System";
    f.seed("Software Engineering and Professional Practice in Computing", longTitle, longName);
    f.assignments.grantExtension(f.as("lect"), f.project, f.ben, hoursAfter(96), std::string(90, 'r'));
    std::string feedback;
    for (int i = 0; i < 30; ++i) feedback += "point" + std::to_string(i) + " ";
    f.markedAt(10, f.ann, 70, feedback);
    f.markedAt(11, f.ben, 60, feedback);
    f.moderation.approve(f.as("hod"), f.submission(f.ann).getId());
    f.moderation.release(f.as("hod"), f.project);

    // Echo only the line breaks, so each line is the program's own text.
    f.run(login("admin") + "1\n2\n2\n5\n5\n1\n0\n0\n", false);
    f.run(login("lect") + "2\n1\n1\n0\n3\n1\n1\n1\n0\n0\n0\n0\n0\n", false);
    f.run(login("hod") + "1\n1\n0\n2\n1\n1\nn\n3\n1\n0\n0\n", false);
    f.run(login("ann") + "1\n3\n1\n0\n0\n", false);
    f.run(login("cat") + "2\n1\n" + f.file("work.pdf") + "\ny\n0\n0\n", false);

    std::istringstream lines(f.allOutput);
    int longLines = 0;
    for (std::string line; std::getline(lines, line);)
        if (line.size() > 80) {
            ++longLines;
            UNSCOPED_INFO("Too wide (" << line.size() << "): " << line);
        }
    CHECK(longLines == 0);
    CHECK_FALSE(contains(f.allOutput, "\x1b["));  // no ANSI colour codes
    // Long text is wrapped, not cut: the student sees all of the feedback (FR-18).
    CHECK(contains(f.allOutput, "point0 "));
    CHECK(contains(f.allOutput, "point29"));
}

// ---------------------------------------------------------------- database errors (real SQLite)
// A trigger added to the test database makes one write fail, as a full disk or a damaged
// file would. Each flow must roll back completely and tell the user.

TEST_CASE("ST-42: UC-09 E4, a database error removes the copied file and stores nothing", "[system]") {
    SystemFixture f;
    f.seed();
    f.clock.set(hoursAfter(10));
    f.db.exec("CREATE TRIGGER fail_submit BEFORE INSERT ON submissions "
              "BEGIN SELECT RAISE(ABORT, 'simulated disk error'); END");
    std::string out = f.run(login("ann") + "2\n1\n" + f.file("report.pdf") + "\ny\n0\n0\n");
    CHECK(contains(out, "simulated disk error"));
    CHECK(contains(out, "The work was not submitted because of an error"));
    CHECK_FALSE(f.submissionRepo.find(f.project, f.ann));
    CHECK(f.storedFiles() == 0);
    CHECK(contains(f.errorLogText(), "simulated disk error"));  // the error is logged
}

TEST_CASE("ST-43: UC-16 E4, a database error keeps neither the mark nor its audit entry", "[system]") {
    SystemFixture f;
    f.seed();
    f.submitAt(10, f.ann);
    f.db.exec("CREATE TRIGGER fail_audit BEFORE INSERT ON audit_log "
              "BEGIN SELECT RAISE(ABORT, 'simulated disk error'); END");
    std::string out = f.run(login("lect") + "3\n1\n1\n1\n75\nWell structured\ny\n0\n0\n0\n");
    CHECK(contains(out, "! Database error: simulated disk error"));
    CHECK_FALSE(contains(out, "Mark saved."));
    CHECK(contains(f.errorLogText(), "simulated disk error"));
    Submission sub = f.submission(f.ann);
    CHECK(sub.getStatus() == SubmissionStatus::Submitted);
    CHECK_FALSE(sub.getMark());
}

TEST_CASE("ST-44: UC-20 E2, a database error during release releases nothing", "[system]") {
    SystemFixture f;
    f.seed();
    f.moderation.approve(f.as("hod"), f.markedAt(10, f.ann, 70));
    f.moderation.approve(f.as("hod"), f.markedAt(11, f.ben, 60));
    // Ben's row is written second, so Ann's has already been changed when the error comes.
    f.db.exec("CREATE TRIGGER fail_release BEFORE UPDATE OF status ON submissions "
              "WHEN NEW.status = 'RELEASED' AND NEW.student_id = " + std::to_string(f.ben) +
              " BEGIN SELECT RAISE(ABORT, 'simulated disk error'); END");
    std::string out = f.run(login("hod") + "2\n1\n1\ny\n0\n0\n");
    CHECK(contains(out, "! Database error: simulated disk error"));
    CHECK_FALSE(contains(out, "mark(s) released."));
    CHECK(contains(f.errorLogText(), "simulated disk error"));
    CHECK(f.submission(f.ann).getStatus() == SubmissionStatus::Moderated);
    CHECK(f.submission(f.ben).getStatus() == SubmissionStatus::Moderated);
}
