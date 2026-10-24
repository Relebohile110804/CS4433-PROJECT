#include "data/LocalFileStore.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <sodium.h>

namespace fs = std::filesystem;

namespace lms::data {

LocalFileStore::LocalFileStore(std::string root) : root_(std::move(root)) {
    if (sodium_init() < 0) throw std::runtime_error("Could not start libsodium");
    fs::create_directories(root_);
}

bool LocalFileStore::isReadableFile(const std::string& sourcePath) {
    std::error_code ec;
    if (!fs::is_regular_file(sourcePath, ec)) return false;
    std::ifstream in(sourcePath, std::ios::binary);
    return in.good();
}

std::uintmax_t LocalFileStore::fileSize(const std::string& sourcePath) { return fs::file_size(sourcePath); }

std::string LocalFileStore::store(const std::string& sourcePath, const std::string& key) {
    fs::path target = fs::path(root_) / (key + fs::path(sourcePath).extension().string());
    if (fs::exists(target)) throw std::runtime_error("A stored file with this name already exists");
    fs::copy_file(sourcePath, target);
    return target.generic_string();
}

std::string LocalFileStore::sha256(const std::string& storedPath) {
    std::ifstream in(storedPath, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read " + storedPath);
    crypto_hash_sha256_state state;
    crypto_hash_sha256_init(&state);
    std::vector<char> buffer(64 * 1024);
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        crypto_hash_sha256_update(&state, reinterpret_cast<const unsigned char*>(buffer.data()),
                                  static_cast<unsigned long long>(in.gcount()));
    }
    unsigned char digest[crypto_hash_sha256_BYTES];
    crypto_hash_sha256_final(&state, digest);
    char hex[crypto_hash_sha256_BYTES * 2 + 1];
    sodium_bin2hex(hex, sizeof hex, digest, sizeof digest);
    return hex;
}

void LocalFileStore::remove(const std::string& storedPath) {
    std::error_code ec;
    fs::remove(storedPath, ec);  // best effort: used while cleaning up after a failure
}

}  // namespace lms::data
