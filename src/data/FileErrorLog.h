#pragma once
#include <string>

#include "business/ports/IClock.h"
#include "business/ports/IErrorLog.h"

namespace lms::data {

// Appends one line per error to a text file, for example lms-errors.log, with the time in UTC.
class FileErrorLog : public ports::IErrorLog {
public:
    FileErrorLog(std::string path, ports::IClock& clock) : path_(std::move(path)), clock_(clock) {}
    void record(const std::string& message) override;

private:
    std::string path_;
    ports::IClock& clock_;
};

}  // namespace lms::data
