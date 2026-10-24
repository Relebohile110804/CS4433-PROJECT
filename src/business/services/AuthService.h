#pragma once
#include "business/ports/IClock.h"
#include "business/ports/IPasswordHasher.h"
#include "business/repositories/IUserRepository.h"
#include "business/services/IAuthService.h"

namespace lms::services {

class AuthService : public IAuthService {
public:
    static constexpr std::size_t MinPasswordLength = 8;  // FR-03

    AuthService(repositories::IUserRepository& users, ports::IPasswordHasher& hasher, ports::IClock& clock)
        : users_(users), hasher_(hasher), clock_(clock) {}

    domain::Session login(const std::string& username, const std::string& password) override;
    void changePassword(const domain::Session& s, const std::string& currentPassword,
                        const std::string& newPassword) override;

    std::string nameOf(const domain::Session& s, int userId) override;

    // Also used by AdminService when it creates accounts or resets passwords.
    static void checkPasswordRules(const std::string& password);

private:
    repositories::IUserRepository& users_;
    ports::IPasswordHasher& hasher_;
    ports::IClock& clock_;
};

}  // namespace lms::services
