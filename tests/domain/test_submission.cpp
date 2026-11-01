#include "catch2/catch_amalgamated.hpp"

#include "../TestHelpers.h"
#include "business/domain/AuditEntry.h"
#include "business/domain/Errors.h"
#include "business/domain/Submission.h"

using namespace lms::domain;
using testutil::base;
using testutil::hash;
using testutil::hoursAfter;

namespace {
SubmissionVersion version(int n, double hours = 0) {
    return SubmissionVersion(n, "submissions/" + std::to_string(n) + ".pdf", hash(), hoursAfter(hours));
}
Mark mark(int raw = 70, double penalty = 0) { return Mark(raw, penalty, raw - penalty, "Good work", 2, base()); }
Submission marked() {
    Submission s = Submission::createNew(1, 10, version(1), 0);
    s.recordMark(mark());
    return s;
}
}  // namespace

TEST_CASE("a new submission is Submitted when on time and Late otherwise (Figure 5)", "[unit][domain]") {
    CHECK(Submission::createNew(1, 10, version(1), 0).getStatus() == SubmissionStatus::Submitted);
    Submission late = Submission::createNew(1, 10, version(1), 2);
    CHECK(late.getStatus() == SubmissionStatus::Late);
    CHECK(late.getDaysLate() == 2);
    CHECK_THROWS_AS(Submission::createNew(1, 10, version(2), 0), RuleViolation);
}

TEST_CASE("resubmission before the deadline adds a version and keeps the old one (BR-04)", "[unit][domain]") {
    Submission s = Submission::createNew(1, 10, version(1), 0);
    s.addVersion(version(2, 1), 0);
    CHECK(s.getVersions().size() == 2);
    CHECK(s.currentVersion().getVersionNo() == 2);
    CHECK(s.getStatus() == SubmissionStatus::Submitted);
    CHECK_THROWS_AS(s.addVersion(version(4), 0), RuleViolation);  // numbers must follow on
}

TEST_CASE("work submitted on time cannot be replaced after the deadline (BR-02)", "[unit][domain]") {
    Submission s = Submission::createNew(1, 10, version(1), 0);
    CHECK_THROWS_AS(s.addVersion(version(2), 1), RuleViolation);
    CHECK(s.getVersions().size() == 1);
}

TEST_CASE("a late submission cannot be replaced (BR-02)", "[unit][domain]") {
    Submission s = Submission::createNew(1, 10, version(1), 1);
    CHECK_THROWS_AS(s.addVersion(version(2), 1), RuleViolation);
}

TEST_CASE("marked work cannot be resubmitted", "[unit][domain]") {
    Submission s = marked();
    CHECK_THROWS_AS(s.addVersion(version(2), 0), RuleViolation);
}

TEST_CASE("full path: mark, approve, release", "[unit][domain]") {
    Submission s = marked();
    CHECK(s.getStatus() == SubmissionStatus::Marked);
    CHECK_FALSE(s.isVisibleToStudent());
    s.approve(3, base());
    CHECK(s.getStatus() == SubmissionStatus::Moderated);
    CHECK(s.getModeratedBy() == 3);
    CHECK_FALSE(s.isVisibleToStudent());
    s.release(base());
    CHECK(s.getStatus() == SubmissionStatus::Released);
    CHECK(s.isVisibleToStudent());
}

TEST_CASE("a late submission can be marked", "[unit][domain]") {
    Submission s = Submission::createNew(1, 10, version(1), 2);
    s.recordMark(mark(70, 20));
    CHECK(s.getStatus() == SubmissionStatus::Marked);
    CHECK(s.getMark()->getFinalMark() == Catch::Approx(50));
}

TEST_CASE("the lecturer can correct a mark before moderation", "[unit][domain]") {
    Submission s = marked();
    s.recordMark(mark(80));
    CHECK(s.getMark()->getRawMark() == 80);
    CHECK(s.getStatus() == SubmissionStatus::Marked);
}

TEST_CASE("return for re-marking needs a comment, then re-marking goes back to Marked (UC-21)", "[unit][domain]") {
    Submission s = marked();
    CHECK_THROWS_AS(s.returnToLecturer("  "), RuleViolation);
    s.returnToLecturer("Check question 3");
    CHECK(s.getStatus() == SubmissionStatus::Returned);
    CHECK(s.getHodComment() == "Check question 3");
    CHECK_THROWS_AS(s.approve(3, base()), RuleViolation);
    s.recordMark(mark(65));
    CHECK(s.getStatus() == SubmissionStatus::Marked);
}

TEST_CASE("HoD adjustment moderates the mark, and is allowed after release (BR-06)", "[unit][domain]") {
    Submission s = marked();
    s.adjust(60, 3, base());
    CHECK(s.getStatus() == SubmissionStatus::Moderated);
    CHECK(s.getMark()->getFinalMark() == Catch::Approx(60));
    s.release(base());
    s.adjust(62, 3, base());
    CHECK(s.getStatus() == SubmissionStatus::Released);
    CHECK(s.getMark()->getFinalMark() == Catch::Approx(62));
}

TEST_CASE("transitions not in the state machine are refused", "[unit][domain]") {
    Submission fresh = Submission::createNew(1, 10, version(1), 0);
    CHECK_THROWS_AS(fresh.approve(3, base()), RuleViolation);
    CHECK_THROWS_AS(fresh.adjust(50, 3, base()), RuleViolation);
    CHECK_THROWS_AS(fresh.returnToLecturer("x"), RuleViolation);
    CHECK_THROWS_AS(fresh.release(base()), RuleViolation);

    Submission s = marked();
    CHECK_THROWS_AS(s.release(base()), RuleViolation);  // only Moderated work is released
    s.approve(3, base());
    CHECK_THROWS_AS(s.recordMark(mark()), RuleViolation);  // lecturer cannot change a moderated mark
    CHECK_THROWS_AS(s.approve(3, base()), RuleViolation);
    s.release(base());
    CHECK_THROWS_AS(s.recordMark(mark()), RuleViolation);
    CHECK_THROWS_AS(s.returnToLecturer("x"), RuleViolation);
    CHECK_THROWS_AS(s.release(base()), RuleViolation);
}

TEST_CASE("mark and version values are checked", "[unit][domain]") {
    CHECK_THROWS_AS(Mark(-1, 0, 0, "fb", 2, base()), RuleViolation);
    CHECK_THROWS_AS(Mark(50, 0, 50, " ", 2, base()), RuleViolation);
    CHECK_THROWS_AS(SubmissionVersion(1, "f.pdf", "abc", base()), RuleViolation);
    CHECK_THROWS_AS(SubmissionVersion(0, "f.pdf", hash(), base()), RuleViolation);
    CHECK_THROWS_AS(SubmissionVersion(1, "f.pdf", std::string(64, 'z'), base()), RuleViolation);
}

TEST_CASE("an audit entry needs a reason and keeps the old value", "[unit][domain]") {
    AuditEntry first(0, 1, 2, base(), std::nullopt, 70, "initial marking");
    CHECK_FALSE(first.getOldValue().has_value());
    AuditEntry change(0, 1, 3, base(), 70.0, 60, "Moderation");
    CHECK(*change.getOldValue() == Catch::Approx(70));
    CHECK_THROWS_AS(AuditEntry(0, 1, 2, base(), std::nullopt, 70, ""), RuleViolation);
}
