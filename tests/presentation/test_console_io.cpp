#include "catch2/catch_amalgamated.hpp"

#include <sstream>

#include "../TestHelpers.h"
#include "presentation/ConsoleIO.h"

using lms::presentation::ConsoleIO;
using lms::presentation::InputClosed;

TEST_CASE("NFR-09: a number prompt asks again until the input is valid", "[unit][ui]") {
    std::istringstream in("abc\n12x\n99\n\n7\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    CHECK(io.number("Choose", 0, 10) == 7);
    CHECK(out.str().find("Please type a whole number from 0 to 10.") != std::string::npos);
}

TEST_CASE("text input is trimmed and required text cannot be empty", "[unit][ui]") {
    std::istringstream in("  hello  \n   \nworld\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    CHECK(io.text("Name") == "hello");
    CHECK(io.requiredText("Name") == "world");
}

TEST_CASE("yes/no and decimal prompts", "[unit][ui]") {
    std::istringstream in("maybe\nY\n-1\n62.5\n");
    std::ostringstream out;
    ConsoleIO io(in, out);
    CHECK(io.confirm("Save"));
    CHECK(io.decimal("Mark", 0, 100) == Catch::Approx(62.5));
}

TEST_CASE("the end of input stops the program instead of looping forever", "[unit][ui]") {
    std::istringstream in("");
    std::ostringstream out;
    ConsoleIO io(in, out);
    CHECK_THROWS_AS(io.number("Choose", 0, 3), InputClosed);
}

TEST_CASE("BR-08: dates are typed and shown in Lesotho time (UTC+2) but kept in UTC", "[unit][ui]") {
    auto t = ConsoleIO::parseDateTime("2026-11-07 17:00");
    REQUIRE(t);
    CHECK(ConsoleIO::formatTime(*t) == "2026-11-07 17:00");
    // 17:00 in Lesotho is 15:00 UTC: 20,764 days after 1970-01-01 plus 15 hours.
    CHECK(t->time_since_epoch() == std::chrono::hours(20764 * 24 + 15));

    CHECK(ConsoleIO::parseDateTime("2028-02-29 00:00"));  // leap year
    CHECK_FALSE(ConsoleIO::parseDateTime("2027-02-29 00:00"));
    CHECK_FALSE(ConsoleIO::parseDateTime("2026-13-01 10:00"));
    CHECK_FALSE(ConsoleIO::parseDateTime("2026-11-31 10:00"));
    CHECK_FALSE(ConsoleIO::parseDateTime("2026-11-07 24:00"));
    CHECK_FALSE(ConsoleIO::parseDateTime("7/11/2026 17:00"));
}

TEST_CASE("NFR-11: marks and long lines are formatted for an 80-column screen", "[unit][ui]") {
    CHECK(ConsoleIO::formatMark(75) == "75");
    CHECK(ConsoleIO::formatMark(62.5) == "62.5");
    CHECK(ConsoleIO::fit(std::string(100, 'x')).size() == 80);
    CHECK(ConsoleIO::fit("short") == "short");
}
