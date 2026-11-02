#pragma once
#include "business/ports/IFileStore.h"

namespace lms::data {

// Keeps submitted files in a folder on the local disk, for example "submissions".
class LocalFileStore : public ports::IFileStore {
public:
    explicit LocalFileStore(std::string root);

    bool isReadableFile(const std::string& sourcePath) override;
    std::uintmax_t fileSize(const std::string& sourcePath) override;
    std::string store(const std::string& sourcePath, const std::string& key) override;
    std::string sha256(const std::string& storedPath) override;
    void remove(const std::string& storedPath) override;

private:
    static constexpr int MaxNameTries = 100;
    std::string root_;
};

}  // namespace lms::data
