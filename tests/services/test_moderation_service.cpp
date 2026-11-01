#include "catch2/catch_amalgamated.hpp"

#include "ServiceFixture.h"
#include "business/domain/Errors.h"

using namespace lms::domain;
using testutil::hoursAfter;

TEST_CASE("FR-16: the HoD's queue lists assignments with work awaiting moderation", "[unit][service]") {
    ServiceFixture f;
    CHECK(f.moderation.queue(f.as(f.hod)).empty());
    f.submitAt(10);
    CHECK(f.moderation.queue(f.as(f.hod)).empty());  // submitted but not marked
    f.markedSubmission();
    auto q = f.moderation.queue(f.as(f.hod));
    REQUIRE(q.size() == 1);
    CHECK(q[0].getTitle() == "Project");
    CHECK(f.moderation.queue(f.as(f.otherHod)).empty());
}

TEST_CASE("FR-16: approve, then FR-17: release makes the mark visible", "[unit][service]") {
    ServiceFixture f;
    int a = f.markedSubmission(70);
    int b = f.markedSubmission(55, f.student2);
    Session hod = f.as(f.hod);
    CHECK(f.moderation.listForModeration(hod, f.assignment).size() == 2);

    f.moderation.approve(hod, a);
    auto sub = f.submissions.findById(a);
    CHECK(sub->getStatus() == SubmissionStatus::Moderated);
    CHECK(*sub->getModeratedBy() == f.hod);

    // Only moderated work is released; b is still waiting.
    CHECK(f.moderation.release(hod, f.assignment) == 1);
    CHECK(f.submissions.findById(a)->getStatus() == SubmissionStatus::Released);
    CHECK(f.submissions.findById(b)->getStatus() == SubmissionStatus::Marked);
    CHECK_THROWS_WITH(f.moderation.release(hod, f.assignment),
                      "There are no moderated marks to release for this assignment");
}

TEST_CASE("FR-16, FR-19: adjusting a mark needs a reason and is audited", "[unit][service]") {
    ServiceFixture f;
    int id = f.markedSubmission(70);
    Session hod = f.as(f.hod);
    std::size_t before = f.auditLog.entries.size();

    CHECK_THROWS_WITH(f.moderation.adjust(hod, id, 75, ""), "A reason is needed to adjust a mark");
    CHECK_THROWS_WITH(f.moderation.adjust(hod, id, 101, "Too generous"), "The new mark must be from 0 to 100");
    f.moderation.adjust(hod, id, 65, "Marking was too generous");

    auto sub = f.submissions.findById(id);
    CHECK(sub->getStatus() == SubmissionStatus::Moderated);
    CHECK(sub->getMark()->getFinalMark() == Catch::Approx(65));
    CHECK(sub->getMark()->getRawMark() == 70);  // the lecturer's raw mark is kept
    REQUIRE(f.auditLog.entries.size() == before + 1);
    const AuditEntry& e = f.auditLog.entries.back();
    CHECK(*e.getOldValue() == Catch::Approx(70));
    CHECK(e.getNewValue() == Catch::Approx(65));
    CHECK(e.getChangedBy() == f.hod);
    CHECK(e.getReason() == "Marking was too generous");
}

TEST_CASE("BR-06: after release only the HoD can change a mark, with a reason", "[unit][service]") {
    ServiceFixture f;
    int id = f.markedSubmission(70);
    Session hod = f.as(f.hod);
    f.moderation.approve(hod, id);
    f.moderation.release(hod, f.assignment);

    f.moderation.adjust(hod, id, 72, "Appeal upheld");
    auto sub = f.submissions.findById(id);
    CHECK(sub->getStatus() == SubmissionStatus::Released);
    CHECK(sub->getMark()->getFinalMark() == Catch::Approx(72));
    CHECK(f.auditLog.entries.back().getReason() == "Appeal upheld");
}

TEST_CASE("FR-21: the HoD returns work to the lecturer, who marks it again", "[unit][service]") {
    ServiceFixture f;
    int id = f.markedSubmission(70);
    Session hod = f.as(f.hod);
    CHECK_THROWS_WITH(f.moderation.returnForRemarking(hod, id, " "), "A comment is needed when returning a mark");
    f.moderation.returnForRemarking(hod, id, "Please check question 3");

    auto sub = f.submissions.findById(id);
    CHECK(sub->getStatus() == SubmissionStatus::Returned);
    CHECK(sub->getHodComment() == "Please check question 3");
    CHECK(f.moderation.queue(hod).empty());  // nothing is awaiting moderation now

    f.marking.mark(f.as(f.lecturer), id, 64, "Question 3 was wrong", "Re-marked after HoD comment");
    CHECK(f.submissions.findById(id)->getStatus() == SubmissionStatus::Marked);
    CHECK(f.moderation.queue(hod).size() == 1);
}

TEST_CASE("BR-05: an HoD can only moderate their own department", "[unit][service]") {
    ServiceFixture f;
    int id = f.markedSubmission();
    Session other = f.as(f.otherHod);
    CHECK_THROWS_AS(f.moderation.approve(other, id), AccessDenied);
    CHECK_THROWS_AS(f.moderation.adjust(other, id, 50, "No"), AccessDenied);
    CHECK_THROWS_AS(f.moderation.release(other, f.assignment), AccessDenied);
    CHECK_THROWS_AS(f.moderation.listForModeration(other, f.assignment), AccessDenied);
    CHECK_THROWS_AS(f.moderation.approve(f.as(f.lecturer), id), AccessDenied);
    CHECK_THROWS_WITH(f.moderation.approve(f.as(f.hod), 999), "Submission not found");
    CHECK_THROWS_WITH(f.moderation.release(f.as(f.hod), 999), "Assignment not found");

    // An HoD without a department is refused too.
    int spare = f.addUser(Role::HeadOfDepartment, "spare", "S102");
    CHECK_THROWS_WITH(f.moderation.queue(f.as(spare)), "You are not the head of any department");
}

TEST_CASE("work that is not marked cannot be approved or adjusted", "[unit][service]") {
    ServiceFixture f;
    int id = f.submitAt(10);
    Session hod = f.as(f.hod);
    CHECK_THROWS_WITH(f.moderation.approve(hod, id), "Cannot approve a submission that is Submitted");
    CHECK_THROWS_WITH(f.moderation.adjust(hod, id, 50, "Why"), "This submission has not been marked yet");
}

TEST_CASE("FR-20: the audit log is limited to the HoD's department", "[unit][service]") {
    ServiceFixture f;
    f.markedSubmission();

    auto all = f.audit.viewLog(f.as(f.admin), std::nullopt, std::nullopt);
    CHECK(all.size() == 1);
    CHECK_FALSE(f.auditLog.lastDepartment);  // the admin sees the whole school

    f.audit.viewLog(f.as(f.hod), f.course, f.assignment);
    CHECK(*f.auditLog.lastDepartment == f.dept);
    CHECK(*f.auditLog.lastCourse == f.course);
    CHECK(*f.auditLog.lastAssignment == f.assignment);

    CHECK_THROWS_AS(f.audit.viewLog(f.as(f.hod), f.otherCourse, std::nullopt), AccessDenied);
    CHECK_THROWS_AS(f.audit.viewLog(f.as(f.lecturer), std::nullopt, std::nullopt), AccessDenied);
    CHECK_THROWS_AS(f.audit.viewLog(f.as(f.student), std::nullopt, std::nullopt), AccessDenied);
    int spare = f.addUser(Role::HeadOfDepartment, "spare", "S102");
    CHECK_THROWS_AS(f.audit.viewLog(f.as(spare), std::nullopt, std::nullopt), AccessDenied);
}
