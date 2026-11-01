#include "data/FileErrorLog.h"

#include <fstream>

#include "data/TimeText.h"

namespace lms::data {

void FileErrorLog::record(const std::string& message) {
    // Best effort: if the log itself cannot be written, the user has still been told.
    std::ofstream out(path_, std::ios::app);
    if (out) out << toIsoText(clock_.now()) << "  " << message << "\n";
}

}  // namespace lms::data
