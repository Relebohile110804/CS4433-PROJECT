#include "data/TimeText.h"

#include <cstdio>
#include <stdexcept>

namespace lms::data {

namespace {

// Days since 1970-01-01 for a civil date, and the reverse (H. Hinnant's algorithms).
// Used instead of timegm/_mkgmtime so the code is the same on Windows and Linux.
long long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
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

}  // namespace

std::string toIsoText(domain::TimePoint t) {
    using namespace std::chrono;
    long long secs = duration_cast<seconds>(t.time_since_epoch()).count();
    long long days = secs >= 0 ? secs / 86400 : (secs - 86399) / 86400;
    long long rest = secs - days * 86400;
    int y;
    unsigned m, d;
    civilFromDays(days, y, m, d);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02uT%02d:%02d:%02dZ", y, m, d, static_cast<int>(rest / 3600),
                  static_cast<int>(rest / 60 % 60), static_cast<int>(rest % 60));
    return buf;
}

domain::TimePoint fromIsoText(const std::string& text) {
    int y, hh, mm, ss;
    unsigned mo, d;
    if (text.size() != 20 || std::sscanf(text.c_str(), "%4d-%2u-%2uT%2d:%2d:%2dZ", &y, &mo, &d, &hh, &mm, &ss) != 6)
        throw std::runtime_error("Bad time in database: " + text);
    long long secs = daysFromCivil(y, mo, d) * 86400 + hh * 3600 + mm * 60 + ss;
    return domain::TimePoint(std::chrono::duration_cast<domain::TimePoint::duration>(std::chrono::seconds(secs)));
}

}  // namespace lms::data
