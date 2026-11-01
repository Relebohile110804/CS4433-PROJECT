#include "catch2/catch_amalgamated.hpp"

#include "ServiceFixture.h"
#include "business/domain/Errors.h"

using namespace lms::domain;
using testutil::hoursAfter;

TEST_CASE("FR-14, FR-22: the lecturer lists submissions with their status", "[unit][service]") {
    ServiceFixture f;
    f.submitAt(10);
    f.submitAt(50, f.student2);
    auto list = f.marking.listSubmissions(f.as(f.lecturer), f.assignment);
    REQUIRE(list.size() == 2);
    CHECK(list[0].getStatus() == SubmissionStatus::Submitted);
    CHECK(list[1].getStatus() == SubmissionStatus::Late);
}

TEST_CASE("FR-15, BR-01: the penalty is applied when marking", "[unit][service]") {
    ServiceFixture f;
    int late = f.submitAt(49, f.student2);  // one hour late: 1 day
    Session lect = f.as(f.lecturer);

    auto p = f.marking.previewMark(lect, late, 70);
    CHECK(p.daysLate == 1);
    CHECK(p.penalty == Catch::Approx(10));
    CHECK(p.finalMark == Catch::Approx(60));

    f.marking.mark(lect, late, 70, "Good but late", "");
    Mark m = *f.submissions.findById(late)->getMark();
    CHECK(m.getRawMark() == 70);
    CHECK(m.getPenalty() == Catch::Approx(10));
    CHECK(m.getFinalMark() == Catch::Approx(60));
    CHECK(m.getMarkedBy() == f.lecturer);
    CHECK(f.submissions.findById(late)->getStatus() == SubmissionStatus::Marked);
}

TEST_CASE("BR-01: more than 3 days late gives 0", "[unit][service]") {
    ServiceFixture f;
    int id = f.submitAt(48 + 72 + 1);
    CHECK(f.marking.previewMark(f.as(f.lecturer), id, 90).finalMark == Catch::Approx(0));
}

TEST_CASE("BR-01, BR-03: an extension granted after a late submission removes the penalty", "[unit][service]") {
    ServiceFixture f;
    int id = f.submitAt(50);
    f.assignments.addExtension(f.assignment, Extension(0, f.student, hoursAfter(60), "Sick", f.lecturer, hoursAfter(55)));
    auto p = f.marking.previewMark(f.as(f.lecturer), id, 70);
    CHECK(p.daysLate == 0);
    CHECK(p.finalMark == Catch::Approx(70));
}

TEST_CASE("FR-19, BR-07: every mark and change gets one audit entry in the same transaction", "[unit][service]") {
    ServiceFixture f;
    int id = f.submitAt(10);
    Session lect = f.as(f.lecturer);
    f.marking.mark(lect, id, 60, "OK", "");
    REQUIRE(f.auditLog.entries.size() == 1);
    CHECK_FALSE(f.auditLog.entries[0].getOldValue());
    CHECK(f.auditLog.entries[0].getNewValue() == Catch::Approx(60));
    CHECK(f.auditLog.entries[0].getReason() == "Initial marking");
    CHECK(f.auditLog.entries[0].getChangedBy() == f.lecturer);

    // A correction before moderation needs a reason.
    CHECK_THROWS_WITH(f.marking.mark(lect, id, 65, "OK", " "), "A reason is needed to change a mark");
    f.marking.mark(lect, id, 65, "Better than I thought", "Missed a section");
    REQUIRE(f.auditLog.entries.size() == 2);
    CHECK(*f.auditLog.entries[1].getOldValue() == Catch::Approx(60));
    CHECK(f.auditLog.entries[1].getNewValue() == Catch::Approx(65));
    CHECK(f.auditLog.entries[1].getReason() == "Missed a section");
}

TEST_CASE("BR-07: if saving the mark fails, no audit entry is written", "[unit][service]") {
    ServiceFixture f;
    int id = f.submitAt(10);
    f.submissions.failNextSave = true;
    CHECK_THROWS(f.marking.mark(f.as(f.lecturer), id, 60, "OK", ""));
    CHECK(f.auditLog.entries.empty());
    CHECK(f.tx.rolledBack == 1);
    CHECK_FALSE(f.submissions.findById(id)->isMarked());
}

TEST_CASE("marking rules", "[unit][service]") {
    ServiceFixture f;
    int id = f.submitAt(10);
    Session lect = f.as(f.lecturer);
    CHECK_THROWS_WITH(f.marking.mark(lect, id, 101, "Too high", ""), "The raw mark must be from 0 to 100");
    CHECK_THROWS_WITH(f.marking.mark(lect, id, -1, "Too low", ""), "The raw mark must be from 0 to 100");
    CHECK_THROWS_WITH(f.marking.mark(lect, id, 50, " ", ""), "Feedback cannot be empty");
    CHECK_THROWS_WITH(f.marking.mark(lect, 999, 50, "OK", ""), "Submission not found");
    CHECK_THROWS_WITH(f.marking.previewMark(lect, 999, 50), "Submission not found");

    int other = f.addUser(Role::Lecturer, "other", "S201");
    CHECK_THROWS_AS(f.marking.mark(f.as(other), id, 50, "OK", ""), AccessDenied);
    CHECK_THROWS_AS(f.marking.listSubmissions(f.as(f.student), f.assignment), AccessDenied);
    CHECK_THROWS_WITH(f.marking.listSubmissions(lect, 999), "Assignment not found");
}

TEST_CASE("BR-06: after moderation the lecturer can no longer change the mark", "[unit][service]") {
    ServiceFixture f;
    int id = f.markedSubmission(70);
    f.moderation.approve(f.as(f.hod), id);
    CHECK_THROWS_WITH(f.marking.mark(f.as(f.lecturer), id, 80, "Changed", "Why not"),
                      "Cannot mark a submission that is Moderated");
}
