#pragma once
#include <cstdint>
#include <string>

namespace lms::ports {

// Where submitted files are kept (the submissions/ folder in Figure 1).
class IFileStore {
public:
    virtual ~IFileStore() = default;
    // True if the path is an existing regular file that can be opened for reading.
    virtual bool isReadableFile(const std::string& sourcePath) = 0;
    virtual std::uintmax_t fileSize(const std::string& sourcePath) = 0;
    // Copies the file into the store under a name made from the key and the
    // original extension, and returns the stored path.
    virtual std::string store(const std::string& sourcePath, const std::string& key) = 0;
    // SHA-256 of the file as 64 lowercase hex characters (FR-11).
    virtual std::string sha256(const std::string& storedPath) = 0;
    // Removes a stored copy, for example when the database transaction fails (UC-09 E4).
    virtual void remove(const std::string& storedPath) = 0;
};

}  // namespace lms::ports
