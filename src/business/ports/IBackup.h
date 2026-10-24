#pragma once
#include <string>

namespace lms::ports {

// FR-23: copy the whole database to a file, or replace it with a copy.
// Implemented by data::Database, so AdminService does not depend on the data layer.
class IBackup {
public:
    virtual ~IBackup() = default;
    virtual void backupTo(const std::string& path) = 0;
    virtual void restoreFrom(const std::string& path) = 0;
};

}  // namespace lms::ports
