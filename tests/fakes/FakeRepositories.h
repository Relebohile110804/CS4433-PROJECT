#pragma once
// In-memory repositories for unit testing the services without SQLite (NFR-12, R12).
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "business/repositories/IUserRepository.h"
#include "business/services/UserFactory.h"

namespace fakes {

class FakeUserRepository : public lms::repositories::IUserRepository {
public:
    std::unique_ptr<lms::domain::User> findById(int id) override {
        auto it = rows_.find(id);
        return it == rows_.end() ? nullptr : copy(*it->second);
    }
    std::unique_ptr<lms::domain::User> findByUsername(const std::string& username) override {
        for (auto& [id, u] : rows_)
            if (u->getUsername() == username) return copy(*u);
        return nullptr;
    }
    bool personNumberExists(const std::string& number) override {
        for (auto& [id, u] : rows_)
            if (u->getPersonNumber() == number) return true;
        return false;
    }
    std::vector<std::unique_ptr<lms::domain::User>> list(std::optional<lms::domain::Role> role) override {
        std::vector<std::unique_ptr<lms::domain::User>> result;
        for (auto& [id, u] : rows_)
            if (!role || u->getRole() == *role) result.push_back(copy(*u));
        return result;
    }
    int add(const lms::domain::User& user) override {
        for (auto& [id, u] : rows_)
            if (u->getUsername() == user.getUsername()) throw std::runtime_error("UNIQUE constraint failed");
        int id = nextId_++;
        auto stored = copy(user);
        stored->setId(id);
        rows_[id] = std::move(stored);
        return id;
    }
    void update(const lms::domain::User& user) override { rows_.at(user.getId()) = copy(user); }

private:
    static std::unique_ptr<lms::domain::User> copy(const lms::domain::User& u) {
        auto c = lms::services::UserFactory::create(u.getRole(), u.getId(), u.getUsername(), u.getFullName(),
                                                    u.getPersonNumber());
        if (!u.getPasswordHash().empty()) c->setPasswordHash(u.getPasswordHash());
        c->restoreLoginState(u.isActive(), u.getFailedLogins(), u.getLockedUntil());
        return c;
    }
    std::map<int, std::unique_ptr<lms::domain::User>> rows_;
    int nextId_ = 1;
};

}  // namespace fakes
