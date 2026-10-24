#pragma once
#include <string>

namespace lms::ports {

// Hides the hashing library from the business layer (NFR-04).
class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;
    // Returns a self-describing hash string that includes its own random salt.
    virtual std::string hash(const std::string& password) = 0;
    virtual bool verify(const std::string& hash, const std::string& password) = 0;
};

}  // namespace lms::ports
