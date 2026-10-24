#pragma once
#include <cstddef>

#include "business/ports/IPasswordHasher.h"

namespace lms::data {

// Argon2id through libsodium (NFR-04). The default limits are libsodium's
// "interactive" ones, chosen to keep a login under 2 seconds on old computers (NFR-01, R11).
class SodiumPasswordHasher : public ports::IPasswordHasher {
public:
    SodiumPasswordHasher();
    SodiumPasswordHasher(unsigned long long opsLimit, std::size_t memLimit);

    std::string hash(const std::string& password) override;
    bool verify(const std::string& hash, const std::string& password) override;

private:
    unsigned long long opsLimit_;
    std::size_t memLimit_;
};

}  // namespace lms::data
