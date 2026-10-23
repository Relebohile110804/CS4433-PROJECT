#pragma once
#include "business/repositories/IUserRepository.h"
#include "data/Database.h"

namespace lms::data {

class SqliteUserRepository : public repositories::IUserRepository {
public:
    explicit SqliteUserRepository(Database& db) : db_(db) {}

    std::unique_ptr<domain::User> findById(int id) override;
    std::unique_ptr<domain::User> findByUsername(const std::string& username) override;
    bool personNumberExists(const std::string& number) override;
    std::vector<std::unique_ptr<domain::User>> list(std::optional<domain::Role> role) override;
    int add(const domain::User& user) override;
    void update(const domain::User& user) override;

private:
    Database& db_;
};

}  // namespace lms::data
