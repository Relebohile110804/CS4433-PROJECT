#include "catch2/catch_amalgamated.hpp"

#include <sstream>
#include <vector>

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

TEST_CASE("R13: a password is read with the echo turned off, then on again", "[unit][ui]") {
    std::istringstream in("   \nsecret123\n");
    std::ostringstream out;
    std::vector<bool> calls;
    ConsoleIO io(in, out, [&](bool on) {
        calls.push_back(on);
        return true;  // as on a real terminal
    });
    CHECK(io.password("Password") == "secret123");
    // Once for the empty try and once for the real one: off, then back on each time.
    CHECK(calls == std::vector<bool>{false, true, false, true});
    CHECK(out.str().find("Please type something.") != std::string::npos);
    CHECK(out.str().find("secret123") == std::string::npos);  // never written to the screen
}

TEST_CASE("R13: the echo comes back on even when the input ends during a password", "[unit][ui]") {
    std::istringstream in("");
    std::ostringstream out;
    std::vector<bool> calls;
    ConsoleIO io(in, out, [&](bool on) {
        calls.push_back(on);
        return true;
    });
    CHECK_THROWS_AS(io.password("Password"), InputClosed);
    CHECK(calls == std::vector<bool>{false, true});
}

TEST_CASE("R13: without a terminal the password is read like other text", "[unit][ui]") {
    std::istringstream in("secret123\n");
    std::ostringstream out;
    ConsoleIO io(in, out, [](bool) { return false; });  // input is a file or pipe
    CHECK(io.password("Password") == "secret123");
    CHECK(out.str() == "Password: ");  // no extra new line when nothing was hidden
    ConsoleIO plain(in, out);
    CHECK_THROWS_AS(plain.password("Password"), InputClosed);
}
