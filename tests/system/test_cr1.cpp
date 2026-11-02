// Change request CR1 (v1.1.0): "A lecturer should be able to extend the deadline for a whole
// class, not just an individual student, and extensions should be shown on the student's
// assignment list." System tests ST-47 to ST-49 and acceptance test AT-08 drive the real
// program through its menus, like the tests in test_system.cpp and test_acceptance.cpp.
#include "SystemFixture.h"

using namespace systest;

namespace {

std::string login(const std::string& user) {
    return "1\n" + user + "\n" + (user == "admin" ? "admin-pass" : "password1") + "\n";
}

// Lecturer > Grant an extension > CS4433 > Project > the whole class (after Ann, Ben and Cat).
const std::string WholeClassPath = "2\n1\n1\n4\n";

int count(const std::string& text, const std::string& part) {
    int n = 0;
    for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) ++n;
    return n;
}

}  // namespace

TEST_CASE("ST-47: CR1, FR-09 a lecturer grants the whole class an extension from the menu", "[system]") {
    SystemFixture f;
    f.seed();
    std::string out = f.run(login("lect") + WholeClassPath + when(f.due) + "\nPower cut\n" +   // not later
                            WholeClassPath + when(hoursAfter(72)) + "\n\nPower cut on Friday\n" +  // no reason first
                            "0\n0\n");
    CHECK(contains(out, "4. The whole class  due " + when(f.due)));
    CHECK(contains(out, "! The new deadline must be later than the current deadline of the class"));
    CHECK(contains(out, "! Please type something."));
    CHECK(contains(out, "Extension granted to the whole class."));
    for (int student : {f.ann, f.ben, f.cat})
        CHECK(f.assignments.effectiveDeadline(f.project, student) == hoursAfter(72));
    CHECK(f.assignments.effectiveDeadline(f.homework, f.ann) == hoursAfter(72));  // other course: its own deadline

    // The student numbers did not move: choice 1 is still Ann (as in ST-08).
    out = f.run(login("lect") + "2\n1\n1\n1\n" + when(hoursAfter(96)) + "\nIllness\n0\n0\n");
    CHECK(contains(out, "Extension granted."));
    CHECK(f.assignments.effectiveDeadline(f.project, f.ann) == hoursAfter(96));
    CHECK(f.assignments.effectiveDeadline(f.project, f.ben) == hoursAfter(72));
}

TEST_CASE("ST-48: CR1, FR-10 the student list shows each extension, who it is for and why", "[system]") {
    SystemFixture f;
    f.seed();
    f.assignments.grantExtension(f.as("lect"), f.project, f.ben, hoursAfter(96), "Illness");
    f.assignments.grantClassExtension(f.as("lect"), f.project, hoursAfter(72), "Power cut");

    std::string ann = f.run(login("ann") + "1\n0\n0\n");
    CHECK(contains(ann, "Due " + when(hoursAfter(72)) + " (extended)"));
    CHECK(contains(ann, "Extended from " + when(f.due) + " for the whole class: Power cut"));
    CHECK(count(ann, "Extended from") == 1);  // the MA1000 homework has no extension

    std::string ben = f.run(login("ben") + "1\n0\n0\n");
    CHECK(contains(ben, "Due " + when(hoursAfter(96)) + " (extended)"));
    CHECK(contains(ben, "Extended from " + when(f.due) + " for you: Illness"));
    CHECK_FALSE(contains(ben, "Power cut"));  // Ben's own, later extension is the one that counts
}

TEST_CASE("ST-49: CR1, UC-20 the HoD sees the whole-class extension and the penalty follows it", "[system]") {
    SystemFixture f;
    f.seed();
    f.assignments.grantClassExtension(f.as("lect"), f.project, hoursAfter(72), "Power cut");
    f.markedAt(60, f.ann, 70);       // late by the original deadline, on time by the class extension
    f.markedAt(74, f.ben, 70);       // 2 hours after the class extension: 1 day late
    std::string out = f.run(login("hod") + "1\n1\n0\n0\n0\n");
    CHECK(contains(out, "Ann Student (2023001)  raw 70, penalty 0, final 70"));
    CHECK(contains(out, "Ben Student (2023002)  raw 70, penalty 10, final 60"));
    CHECK(contains(out, "Whole-class extension to " + when(hoursAfter(72)) + ": Power cut"));
}

TEST_CASE("AT-08: School (CR1), one extension for the whole class reaches every student", "[acceptance]") {
    SystemFixture f;
    f.seed();
    // The lecturer needs the same number of choices as for one student (AT-04), so NFR-10 still holds.
    std::string out = f.run(login("lect") + WholeClassPath + when(hoursAfter(72)) + "\nSchool sports day\n0\n0\n");
    CHECK(contains(out, "Extension granted to the whole class."));
    for (const char* who : {"ann", "ben", "cat"})
        CHECK(contains(f.run(login(who) + "1\n0\n0\n"), "for the whole class: School sports day"));
    // Work handed in after the original deadline but before the new one is on time.
    f.submitAt(60, f.cat);
    CHECK(f.submission(f.cat).getStatus() == SubmissionStatus::Submitted);
}
