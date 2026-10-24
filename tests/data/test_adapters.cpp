#include "doctest/doctest.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "data/LocalFileStore.h"
#include "data/SodiumPasswordHasher.h"
#include "data/SystemClock.h"

using namespace lms::data;
namespace fs = std::filesystem;

namespace {
// A scratch folder that is deleted when the test ends.
struct TempDir {
    fs::path path;
    TempDir() : path(fs::temp_directory_path() / "lms_adapter_test") {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    std::string write(const std::string& name, const std::string& content) {
        std::ofstream(path / name, std::ios::binary) << content;
        return (path / name).string();
    }
};
}  // namespace

TEST_CASE("Argon2id hash verifies the right password only (NFR-04)") {
    SodiumPasswordHasher hasher;
    std::string h = hasher.hash("Secret123");
    CHECK(h.rfind("$argon2id$", 0) == 0);  // the algorithm is recorded in the hash
    CHECK(h.find("Secret123") == std::string::npos);
    CHECK(hasher.verify(h, "Secret123"));
    CHECK_FALSE(hasher.verify(h, "secret123"));
    CHECK_FALSE(hasher.verify("not a hash", "Secret123"));
}

TEST_CASE("the same password gives a different hash each time (random salt)") {
    SodiumPasswordHasher hasher;
    CHECK(hasher.hash("Secret123") != hasher.hash("Secret123"));
}

TEST_CASE("hashing plus verifying takes under 2 seconds (NFR-01, R11)") {
    SodiumPasswordHasher hasher;
    auto start = std::chrono::steady_clock::now();
    std::string h = hasher.hash("Secret123");
    bool ok = hasher.verify(h, "Secret123");
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    MESSAGE("Argon2id hash + verify took " << ms << " ms");
    CHECK(ok);
    CHECK(ms < 2000);
}

TEST_CASE("file store copies the file and keeps its extension") {
    TempDir tmp;
    std::string source = tmp.write("essay.pdf", "my essay");
    LocalFileStore store((tmp.path / "submissions").string());

    CHECK(store.isReadableFile(source));
    CHECK(store.fileSize(source) == 8);
    std::string stored = store.store(source, "a1_s10_v1");
    CHECK(stored.size() > 4);
    CHECK(stored.substr(stored.size() - 4) == ".pdf");
    CHECK(fs::exists(stored));
    CHECK_THROWS(store.store(source, "a1_s10_v1"));  // never overwrites a stored version

    store.remove(stored);
    CHECK_FALSE(fs::exists(stored));
    CHECK_NOTHROW(store.remove(stored));  // removing twice is harmless
}

TEST_CASE("missing files and folders are not readable files") {
    TempDir tmp;
    LocalFileStore store((tmp.path / "submissions").string());
    CHECK_FALSE(store.isReadableFile((tmp.path / "nothing.pdf").string()));
    CHECK_FALSE(store.isReadableFile(tmp.path.string()));
}

TEST_CASE("SHA-256 matches the known value (FR-11)") {
    TempDir tmp;
    LocalFileStore store((tmp.path / "submissions").string());
    CHECK(store.sha256(tmp.write("abc.txt", "abc")) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(store.sha256(tmp.write("empty.txt", "")) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK_THROWS(store.sha256((tmp.path / "missing.txt").string()));
}

TEST_CASE("system clock is close to the real time") {
    SystemClock clock;
    auto diff = std::chrono::system_clock::now() - clock.now();
    CHECK(std::chrono::abs(diff) < std::chrono::seconds(1));
}
