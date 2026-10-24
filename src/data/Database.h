#pragma once
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "business/ports/IBackup.h"
#include "business/repositories/ITransactionManager.h"

struct sqlite3;
struct sqlite3_stmt;

namespace lms::data {

class DatabaseError : public std::runtime_error {
public:
    explicit DatabaseError(const std::string& message) : std::runtime_error(message) {}
};

class Database;

// A prepared SQL statement. Values can only be passed in through the bind
// operations, so every query is parameterised (NFR-06). Parameters are numbered from 1.
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    Statement(Statement&& other) noexcept;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement& operator=(Statement&&) = delete;
    ~Statement();

    Statement& bindInt(int index, long long value);
    Statement& bindDouble(int index, double value);
    Statement& bindText(int index, const std::string& value);
    Statement& bindNull(int index);
    Statement& bindOptionalInt(int index, const std::optional<int>& value);
    Statement& bindOptionalDouble(int index, const std::optional<double>& value);

    // Runs one step. Returns true while there is a row to read.
    bool step();
    // Runs a statement that returns no rows (INSERT, UPDATE, DELETE).
    void run();

    // Columns are numbered from 0.
    int columnInt(int col) const;
    double columnDouble(int col) const;
    std::string columnText(int col) const;
    bool isNull(int col) const;

private:
    void check(int rc) const;
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

// Rolls back to its savepoint when destroyed without commit(). Savepoints nest,
// so a repository can use one inside a service's transaction.
class Transaction : public repositories::ITransaction {
public:
    explicit Transaction(Database& db);
    ~Transaction() override;
    void commit() override;

private:
    Database& db_;
    std::string name_;
    bool committed_ = false;
};

// The connection to lms.db. Creates the tables from schema.sql when the file is new.
class Database : public repositories::ITransactionManager, public ports::IBackup {
public:
    // ":memory:" opens an in-memory database (used by the tests).
    explicit Database(const std::string& path);
    ~Database() override;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Statement prepare(const std::string& sql);
    void exec(const std::string& sql);
    long long lastInsertId() const;
    std::unique_ptr<repositories::ITransaction> begin() override;

    // FR-23: copy the whole database to a file, or replace it with a copy.
    void backupTo(const std::string& path) override;
    void restoreFrom(const std::string& path) override;

    const std::string& getPath() const { return path_; }

private:
    friend class Transaction;
    void createSchemaIfNew();
    sqlite3* handle_ = nullptr;
    std::string path_;
    int nextSavepoint_ = 0;
};

}  // namespace lms::data
