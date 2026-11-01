#include "catch2/catch_amalgamated.hpp"

#include "../TestHelpers.h"
#include "business/services/TenPercentPerDayPolicy.h"

using lms::services::TenPercentPerDayPolicy;
using testutil::hoursAfter;

TEST_CASE("BR-01: days late are started 24-hour periods after the deadline", "[unit][service]") {
    TenPercentPerDayPolicy p;
    auto deadline = hoursAfter(0);
    auto second = std::chrono::seconds(1);
    CHECK(p.daysLate(deadline, hoursAfter(-1)) == 0);
    CHECK(p.daysLate(deadline, deadline) == 0);               // exactly on the deadline
    CHECK(p.daysLate(deadline, deadline + second) == 1);      // one second late
    CHECK(p.daysLate(deadline, hoursAfter(24)) == 1);
    CHECK(p.daysLate(deadline, hoursAfter(24) + second) == 2);
    CHECK(p.daysLate(deadline, hoursAfter(72)) == 3);
    CHECK(p.daysLate(deadline, hoursAfter(72) + second) == 4);  // more than 3 days
}

TEST_CASE("BR-01: 10% of the maximum mark per day, up to 3 days, then 0", "[unit][service]") {
    TenPercentPerDayPolicy p;
    CHECK(p.finalMark(70, 100, 0) == Catch::Approx(70));
    CHECK(p.finalMark(70, 100, 1) == Catch::Approx(60));
    CHECK(p.finalMark(70, 100, 2) == Catch::Approx(50));
    CHECK(p.finalMark(70, 100, 3) == Catch::Approx(40));
    CHECK(p.finalMark(70, 100, 4) == Catch::Approx(0));
    CHECK(p.finalMark(15, 50, 3) == Catch::Approx(0));  // never below 0: 15 - 15 = 0
    CHECK(p.finalMark(10, 50, 3) == Catch::Approx(0));  // 10 - 15 would be negative
    CHECK(p.finalMark(40, 50, 1) == Catch::Approx(35));  // 10% of the maximum (50), not of the raw mark
}
