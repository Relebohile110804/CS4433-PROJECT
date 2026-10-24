#pragma once
// Test doubles for the ports, so services can be unit tested without the
// real clock, libsodium or the disk (NFR-12, R12).
#include <map>
#include <string>

#include "business/ports/IClock.h"
#include "business/ports/IFileStore.h"
#include "business/ports/IPasswordHasher.h"

namespace fakes {

// A clock that only moves when the test says so.
class FakeClock : public lms::ports::IClock {
public:
    explicit FakeClock(lms::domain::TimePoint start) : now_(start) {}
    lms::domain::TimePoint now() override { return now_; }
    void set(lms::domain::TimePoint t) { now_ = t; }
    void advance(std::chrono::seconds s) { now_ += s; }

private:
    lms::domain::TimePoint now_;
};

// Fast and readable stand-in for Argon2id: "hashed:" + password.
class FakePasswordHasher : public lms::ports::IPasswordHasher {
public:
    std::string hash(const std::string& password) override { return "hashed:" + password; }
    bool verify(const std::string& hash, const std::string& password) override { return hash == "hashed:" + password; }
};

// Files live in a map instead of on disk. Tests add source files with addSource().
class FakeFileStore : public lms::ports::IFileStore {
public:
    void addSource(const std::string& path, std::uintmax_t size) { sources_[path] = size; }
    bool isReadableFile(const std::string& path) override { return sources_.count(path) > 0; }
    std::uintmax_t fileSize(const std::string& path) override { return sources_.at(path); }
    std::string store(const std::string& sourcePath, const std::string& key) override {
        std::string ext = sourcePath.substr(sourcePath.find_last_of('.'));
        std::string stored = "submissions/" + key + ext;
        stored_[stored] = sourcePath;
        return stored;
    }
    std::string sha256(const std::string&) override { return std::string(64, 'a'); }
    void remove(const std::string& storedPath) override { stored_.erase(storedPath); }
    bool isStored(const std::string& storedPath) const { return stored_.count(storedPath) > 0; }
    std::size_t storedCount() const { return stored_.size(); }

private:
    std::map<std::string, std::uintmax_t> sources_;
    std::map<std::string, std::string> stored_;
};

}  // namespace fakes
