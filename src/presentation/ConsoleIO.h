#pragma once
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "business/domain/Time.h"

namespace lms::presentation {

// Thrown when the input ends (Ctrl+Z / Ctrl+D), so the program can close cleanly.
class InputClosed : public std::runtime_error {
public:
    InputClosed() : std::runtime_error("Input closed") {}
};

// Console I/O (Figure 1): every prompt checks its input and asks again until it is valid,
// so a typing mistake never crashes the program (NFR-09). Output stays within 80 columns (NFR-11).
class ConsoleIO {
public:
    static constexpr int Width = 80;
    // Times are stored in UTC (BR-08) and shown in Lesotho time, which is UTC+2 all year.
    static constexpr std::chrono::hours UtcOffset{2};

    ConsoleIO(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

    void heading(const std::string& title);
    void say(const std::string& line);
    void error(const std::string& message);

    // Text with spaces trimmed; may be empty.
    std::string text(const std::string& prompt);
    // Text that must not be empty.
    std::string requiredText(const std::string& prompt);
    int number(const std::string& prompt, int min, int max);
    double decimal(const std::string& prompt, double min, double max);
    bool confirm(const std::string& prompt);
    // A date and time typed as YYYY-MM-DD HH:MM in Lesotho time.
    domain::TimePoint dateTime(const std::string& prompt);

    // Shows numbered options and returns the choice: 1..n, or 0 for the last line (Back, Cancel or Log out).
    int menu(const std::string& title, const std::vector<std::string>& options, const std::string& zeroLabel = "Back");

    static std::string formatTime(domain::TimePoint t);
    static std::optional<domain::TimePoint> parseDateTime(const std::string& text);
    static std::string formatMark(double mark);
    // Cuts text that would run past the given width.
    static std::string fit(const std::string& text, std::size_t width = Width);

private:
    std::string readLine();
    std::istream& in_;
    std::ostream& out_;
};

}  // namespace lms::presentation
