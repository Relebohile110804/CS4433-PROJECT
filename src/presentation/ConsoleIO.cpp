#include "presentation/ConsoleIO.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <regex>

namespace lms::presentation {

namespace {

std::string trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Days since 1970-01-01 for a calendar date, and back (H. Hinnant's civil date algorithms).
long long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long long>(era) * 146097 + static_cast<long long>(doe) - 719468;
}

void civilFromDays(long long z, int& y, unsigned& m, unsigned& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y = static_cast<int>(yoe + era * 400) + (m <= 2);
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

unsigned daysInMonth(int y, unsigned m) {
    static const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && isLeap(y) ? 29 : days[m - 1];
}

}  // namespace

std::string ConsoleIO::readLine() {
    std::string line;
    if (!std::getline(in_, line)) throw InputClosed();
    return line;
}

void ConsoleIO::heading(const std::string& title) { out_ << "\n" << fit("== " + title + " ==") << "\n"; }

void ConsoleIO::say(const std::string& line) {
    for (const std::string& part : wrap(line)) out_ << part << "\n";
}

void ConsoleIO::error(const std::string& message) { say("! " + message); }

std::string ConsoleIO::text(const std::string& prompt) {
    std::vector<std::string> lines = wrap(prompt + ":");
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) out_ << lines[i] << "\n";
    out_ << lines.back() << " " << std::flush;
    return trim(readLine());
}

std::string ConsoleIO::requiredText(const std::string& prompt) {
    for (;;) {
        std::string value = text(prompt);
        if (!value.empty()) return value;
        error("Please type something.");
    }
}

int ConsoleIO::number(const std::string& prompt, int min, int max) {
    for (;;) {
        std::string value = text(prompt + " (" + std::to_string(min) + "-" + std::to_string(max) + ")");
        try {
            std::size_t used = 0;
            int n = std::stoi(value, &used);
            if (used == value.size() && n >= min && n <= max) return n;
        } catch (const std::exception&) {
        }
        error("Please type a whole number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
    }
}

double ConsoleIO::decimal(const std::string& prompt, double min, double max) {
    for (;;) {
        std::string value = text(prompt + " (" + formatMark(min) + "-" + formatMark(max) + ")");
        try {
            std::size_t used = 0;
            double n = std::stod(value, &used);
            if (used == value.size() && n >= min && n <= max) return n;
        } catch (const std::exception&) {
        }
        error("Please type a number from " + formatMark(min) + " to " + formatMark(max) + ".");
    }
}

bool ConsoleIO::confirm(const std::string& prompt) {
    for (;;) {
        std::string value = text(prompt + " (y/n)");
        if (value == "y" || value == "Y" || value == "yes") return true;
        if (value == "n" || value == "N" || value == "no") return false;
        error("Please type y or n.");
    }
}

domain::TimePoint ConsoleIO::dateTime(const std::string& prompt) {
    for (;;) {
        std::optional<domain::TimePoint> t = parseDateTime(text(prompt + " (YYYY-MM-DD HH:MM)"));
        if (t) return *t;
        error("Please type a real date and time, for example 2026-11-07 17:00.");
    }
}

int ConsoleIO::menu(const std::string& title, const std::vector<std::string>& options, const std::string& zeroLabel) {
    heading(title);
    for (std::size_t i = 0; i < options.size(); ++i) say(fit("  " + std::to_string(i + 1) + ". " + options[i]));
    say("  0. " + zeroLabel);
    return number("Choose", 0, static_cast<int>(options.size()));
}

int ConsoleIO::detailedMenu(const std::string& title, const std::vector<Option>& options, const std::string& zeroLabel) {
    heading(title);
    for (std::size_t i = 0; i < options.size(); ++i) {
        say(fit("  " + std::to_string(i + 1) + ". " + options[i].line));
        for (const std::string& d : options[i].details) say("     " + d);
    }
    say("  0. " + zeroLabel);
    return number("Choose", 0, static_cast<int>(options.size()));
}

std::optional<domain::TimePoint> ConsoleIO::parseDateTime(const std::string& text) {
    static const std::regex pattern(R"((\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}))");
    std::smatch m;
    if (!std::regex_match(text, m, pattern)) return std::nullopt;
    int y = std::stoi(m[1]);
    unsigned mo = static_cast<unsigned>(std::stoi(m[2]));
    unsigned d = static_cast<unsigned>(std::stoi(m[3]));
    int h = std::stoi(m[4]);
    int mi = std::stoi(m[5]);
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > daysInMonth(y, mo) || h > 23 || mi > 59) return std::nullopt;
    auto local = std::chrono::hours(24 * daysFromCivil(y, mo, d) + h) + std::chrono::minutes(mi);
    return domain::TimePoint(std::chrono::duration_cast<domain::TimePoint::duration>(local - UtcOffset));
}

std::string ConsoleIO::formatTime(domain::TimePoint t) {
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(t.time_since_epoch() + UtcOffset).count();
    long long days = minutes / (24 * 60);
    long long rest = minutes % (24 * 60);
    if (rest < 0) {
        rest += 24 * 60;
        --days;
    }
    int y;
    unsigned m, d;
    civilFromDays(days, y, m, d);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02u %02d:%02d", y, m, d, static_cast<int>(rest / 60),
                  static_cast<int>(rest % 60));
    return buf;
}

std::string ConsoleIO::formatMark(double mark) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", mark);
    std::string s = buf;
    if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0) s.erase(s.size() - 2);
    return s;
}

std::string ConsoleIO::fit(const std::string& text, std::size_t width) {
    if (text.size() <= width) return text;
    return text.substr(0, width - 3) + "...";
}

std::vector<std::string> ConsoleIO::wrap(const std::string& text, std::size_t width) {
    std::size_t lead = text.find_first_not_of(' ');
    std::string indent(std::min<std::size_t>(lead == std::string::npos ? 0 : lead, width / 2) + 4, ' ');
    std::vector<std::string> lines;
    std::string rest = text;
    while (rest.size() > width) {
        std::size_t cut = rest.find_last_of(' ', width);
        bool atSpace = cut != std::string::npos && cut > indent.size();
        if (!atSpace) cut = width;  // one long word: cut it
        std::string line = rest.substr(0, cut);
        line.erase(line.find_last_not_of(' ') + 1);
        lines.push_back(line);
        std::size_t next = rest.find_first_not_of(' ', atSpace ? cut + 1 : cut);
        if (next == std::string::npos) return lines;
        rest = indent + rest.substr(next);
    }
    lines.push_back(rest);
    return lines;
}

}  // namespace lms::presentation
