#include "catch2/catch_amalgamated.hpp"

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

TEST_CASE("Argon2id hash verifies the right password only (NFR-04)", "[integration][data]") {
    SodiumPasswordHasher hasher;
    std::string h = hasher.hash("Secret123");
    CHECK(h.rfind("$argon2id$", 0) == 0);  // the algorithm is recorded in the hash
    CHECK(h.find("Secret123") == std::string::npos);
    CHECK(hasher.verify(h, "Secret123"));
    CHECK_FALSE(hasher.verify(h, "secret123"));
    CHECK_FALSE(hasher.verify("not a hash", "Secret123"));
}

TEST_CASE("the same password gives a different hash each time (random salt)", "[integration][data]") {
    SodiumPasswordHasher hasher;
    CHECK(hasher.hash("Secret123") != hasher.hash("Secret123"));
}

TEST_CASE("hashing plus verifying takes under 2 seconds (NFR-01, R11)", "[integration][data]") {
    SodiumPasswordHasher hasher;
    auto start = std::chrono::steady_clock::now();
    std::string h = hasher.hash("Secret123");
    bool ok = hasher.verify(h, "Secret123");
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    WARN("Argon2id hash + verify took " << ms << " ms");
    CHECK(ok);
    CHECK(ms < 2000);
}

TEST_CASE("file store copies the file and keeps its extension", "[integration][data]") {
    TempDir tmp;
    std::string source = tmp.write("essay.pdf", "my essay");
    LocalFileStore store((tmp.path / "submissions").string());

    CHECK(store.isReadableFile(source));
    CHECK(store.fileSize(source) == 8);
    std::string stored = store.store(source, "a1_s10_v1");
    CHECK(stored.size() > 4);
    CHECK(stored.substr(stored.size() - 4) == ".pdf");
    CHECK(fs::exists(stored));
    // A name that is taken (for example a copy left by a crash) is never overwritten (CR-01).
    std::string again = store.store(tmp.write("essay2.pdf", "second copy"), "a1_s10_v1");
    CHECK(again != stored);
    CHECK(fs::path(again).filename() == "a1_s10_v1_2.pdf");
    CHECK(store.sha256(stored) == store.sha256(source));
    store.remove(again);

    store.remove(stored);
    CHECK_FALSE(fs::exists(stored));
    CHECK_NOTHROW(store.remove(stored));  // removing twice is harmless
}

TEST_CASE("missing files and folders are not readable files", "[integration][data]") {
    TempDir tmp;
    LocalFileStore store((tmp.path / "submissions").string());
    CHECK_FALSE(store.isReadableFile((tmp.path / "nothing.pdf").string()));
    CHECK_FALSE(store.isReadableFile(tmp.path.string()));
}

TEST_CASE("SHA-256 matches the known value (FR-11)", "[integration][data]") {
    TempDir tmp;
    LocalFileStore store((tmp.path / "submissions").string());
    CHECK(store.sha256(tmp.write("abc.txt", "abc")) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(store.sha256(tmp.write("empty.txt", "")) ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK_THROWS(store.sha256((tmp.path / "missing.txt").string()));
}

TEST_CASE("system clock is close to the real time", "[integration][data]") {
    SystemClock clock;
    auto diff = std::chrono::system_clock::now() - clock.now();
    CHECK(std::chrono::abs(diff) < std::chrono::seconds(1));
}
