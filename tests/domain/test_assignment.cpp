#include "catch2/catch_amalgamated.hpp"

#include "../TestHelpers.h"
#include "business/domain/Assignment.h"
#include "business/domain/Course.h"
#include "business/domain/Errors.h"

using namespace lms::domain;
using testutil::base;
using testutil::hoursAfter;

TEST_CASE("maximum mark must be 1 to 1000 (FR-08)", "[unit][domain]") {
    CHECK_NOTHROW(Assignment(0, 1, "A1", "", base(), 1));
    CHECK_NOTHROW(Assignment(0, 1, "A1", "", base(), 1000));
    CHECK_THROWS_AS(Assignment(0, 1, "A1", "", base(), 0), RuleViolation);
    CHECK_THROWS_AS(Assignment(0, 1, "A1", "", base(), 1001), RuleViolation);
    CHECK_THROWS_AS(Assignment(0, 1, "  ", "", base(), 50), RuleViolation);
}

TEST_CASE("effective deadline uses the student's extension (BR-03)", "[unit][domain]") {
    Assignment a(1, 1, "A1", "", hoursAfter(48), 100);
    a.addExtension(Extension(0, 10, hoursAfter(96), "Illness", 2, base()), base());
    CHECK(a.effectiveDeadlineFor(10) == hoursAfter(96));
    CHECK(a.effectiveDeadlineFor(11) == hoursAfter(48));  // other students keep the original deadline
}

TEST_CASE("an extension must be later than the current effective deadline", "[unit][domain]") {
    Assignment a(1, 1, "A1", "", hoursAfter(48), 100);
    CHECK_THROWS_AS(a.addExtension(Extension(0, 10, hoursAfter(48), "Same time", 2, base()), base()), RuleViolation);
    a.addExtension(Extension(0, 10, hoursAfter(72), "Illness", 2, base()), base());
    CHECK_THROWS_AS(a.addExtension(Extension(0, 10, hoursAfter(60), "Shorter", 2, base()), base()), RuleViolation);
    a.addExtension(Extension(0, 10, hoursAfter(100), "Longer illness", 2, base()), base());
    CHECK(a.effectiveDeadlineFor(10) == hoursAfter(100));
}

TEST_CASE("an extension can be granted after the original deadline but not into the past", "[unit][domain]") {
    Assignment a(1, 1, "A1", "", hoursAfter(24), 100);
    TimePoint now = hoursAfter(30);
    CHECK_NOTHROW(a.addExtension(Extension(0, 10, hoursAfter(48), "Illness", 2, now), now));
    CHECK_THROWS_AS(a.addExtension(Extension(0, 11, hoursAfter(28), "Too late", 2, now), now), RuleViolation);
}

TEST_CASE("an extension needs a reason", "[unit][domain]") {
    CHECK_THROWS_AS(Extension(0, 10, hoursAfter(48), " ", 2, base()), RuleViolation);
}

TEST_CASE("course keeps its lecturers and students", "[unit][domain]") {
    Course c(1, 1, "CS4433", "Software Engineering");
    c.addLecturer(2);
    c.enrol(10);
    CHECK(c.isTaughtBy(2));
    CHECK(c.hasStudent(10));
    c.removeLecturer(2);
    c.withdraw(10);
    CHECK_FALSE(c.isTaughtBy(2));
    CHECK_FALSE(c.hasStudent(10));
    CHECK_THROWS_AS(c.rename("", "Title"), RuleViolation);
}
