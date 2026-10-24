#include "data/SodiumPasswordHasher.h"

#include <stdexcept>

#include <sodium.h>

namespace lms::data {

SodiumPasswordHasher::SodiumPasswordHasher()
    : SodiumPasswordHasher(crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) {}

SodiumPasswordHasher::SodiumPasswordHasher(unsigned long long opsLimit, std::size_t memLimit)
    : opsLimit_(opsLimit), memLimit_(memLimit) {
    if (sodium_init() < 0) throw std::runtime_error("Could not start libsodium");
}

std::string SodiumPasswordHasher::hash(const std::string& password) {
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str_alg(out, password.c_str(), password.size(), opsLimit_, memLimit_,
                              crypto_pwhash_ALG_ARGON2ID13) != 0)
        throw std::runtime_error("Not enough memory to hash the password");
    return out;
}

bool SodiumPasswordHasher::verify(const std::string& hash, const std::string& password) {
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

}  // namespace lms::data
