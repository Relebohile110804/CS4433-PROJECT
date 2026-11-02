#include "catch2/catch_amalgamated.hpp"

#include <cstdio>
#include <filesystem>

#include "../TestHelpers.h"
#include "data/Database.h"
#include "data/TimeText.h"

using namespace lms::data;
using testutil::hoursAfter;

namespace {
int countRows(Database& db, const std::string& table) {
    Statement st = db.prepare("SELECT count(*) FROM " + table);
    st.step();
    return st.columnInt(0);
}
}  // namespace

TEST_CASE("a new database gets all 12 tables from schema.sql", "[integration][data]") {
    Database db(":memory:");
    Statement st = db.prepare("SELECT count(*) FROM sqlite_master WHERE type = 'table'");
    st.step();
    CHECK(st.columnInt(0) == 12);  // 11 in v1.0.0, plus class_extensions in v1.1.0 (CR1)
}

TEST_CASE("foreign keys are switched on", "[integration][data]") {
    Database db(":memory:");
    CHECK_THROWS_AS(db.exec("INSERT INTO courses (department_id, code, title) VALUES (99, 'X', 'Y')"), DatabaseError);
}

TEST_CASE("time text round trip in UTC (BR-08)", "[integration][data]") {
    CHECK(toIsoText(fromIsoText("2026-11-02T08:30:00Z")) == "2026-11-02T08:30:00Z");
    CHECK(toIsoText(fromIsoText("2024-02-29T23:59:59Z")) == "2024-02-29T23:59:59Z");
    CHECK(fromIsoText("2026-11-02T08:30:01Z") - fromIsoText("2026-11-02T08:30:00Z") == std::chrono::seconds(1));
    CHECK(fromIsoText(toIsoText(hoursAfter(5))) == hoursAfter(5));
    CHECK_THROWS(fromIsoText("02/11/2026"));
}

TEST_CASE("bound text is stored as data, not run as SQL (NFR-06)", "[integration][data]") {
    Database db(":memory:");
    std::string evil = "x'); DROP TABLE users; --";
    db.prepare("INSERT INTO users (username, password_hash, full_name, role, person_number) "
               "VALUES (?1, 'h', 'Name', 'STUDENT', '1')")
        .bindText(1, evil)
        .run();
    Statement st = db.prepare("SELECT username FROM users");
    REQUIRE(st.step());
    CHECK(st.columnText(0) == evil);
}

TEST_CASE("a transaction rolls back unless committed (NFR-08)", "[integration][data]") {
    Database db(":memory:");
    const char* insert = "INSERT INTO users (username, password_hash, full_name, role, person_number) "
                         "VALUES ('u1', 'h', 'N', 'STUDENT', '1')";
    {
        auto tx = db.begin();
        db.exec(insert);
    }
    CHECK(countRows(db, "users") == 0);
    {
        auto tx = db.begin();
        db.exec(insert);
        tx->commit();
    }
    CHECK(countRows(db, "users") == 1);
}

TEST_CASE("nested transactions: an inner rollback keeps the outer work", "[integration][data]") {
    Database db(":memory:");
    auto outer = db.begin();
    db.exec("INSERT INTO users (username, password_hash, full_name, role, person_number) "
            "VALUES ('a', 'h', 'N', 'STUDENT', '1')");
    {
        auto inner = db.begin();
        db.exec("INSERT INTO users (username, password_hash, full_name, role, person_number) "
                "VALUES ('b', 'h', 'N', 'STUDENT', '2')");
    }
    outer->commit();
    CHECK(countRows(db, "users") == 1);
}

TEST_CASE("audit entries cannot be changed or deleted, even with direct SQL (BR-07)", "[integration][data]") {
    Database db(":memory:");
    db.exec("INSERT INTO users VALUES (1, 's', 'h', 'S', 'STUDENT', '1', 1, 0, NULL);"
            "INSERT INTO users VALUES (2, 'h', 'h', 'H', 'HOD', '2', 1, 0, NULL);"
            "INSERT INTO departments VALUES (1, 'CS', 2);"
            "INSERT INTO courses VALUES (1, 1, 'CS1', 'Intro');"
            "INSERT INTO assignments VALUES (1, 1, 'A1', '', '2026-11-01T00:00:00Z', 100);"
            "INSERT INTO submissions (submission_id, assignment_id, student_id, status) VALUES (1, 1, 1, 'MARKED');"
            "INSERT INTO audit_log VALUES (1, 1, 2, '2026-11-02T00:00:00Z', NULL, 70, 'initial marking');");
    CHECK_THROWS_AS(db.exec("UPDATE audit_log SET new_value = 100"), DatabaseError);
    CHECK_THROWS_AS(db.exec("DELETE FROM audit_log"), DatabaseError);
    CHECK(countRows(db, "audit_log") == 1);
}

TEST_CASE("backup and restore (FR-23)", "[integration][data]") {
    auto dir = std::filesystem::temp_directory_path();
    std::string backupFile = (dir / "lms_test_backup.db").string();
    std::string notBackup = (dir / "lms_test_not_backup.db").string();
    std::filesystem::remove(backupFile);
    std::filesystem::remove(notBackup);

    Database db(":memory:");
    db.exec("INSERT INTO users (username, password_hash, full_name, role, person_number) "
            "VALUES ('keep', 'h', 'N', 'STUDENT', '1')");
    db.backupTo(backupFile);
    db.exec("INSERT INTO users (username, password_hash, full_name, role, person_number) "
            "VALUES ('later', 'h', 'N', 'STUDENT', '2')");
    CHECK(countRows(db, "users") == 2);

    db.restoreFrom(backupFile);
    CHECK(countRows(db, "users") == 1);

    {
        // An SQLite file that is not an LMS database must be refused.
        Database other(notBackup);
        other.exec("DROP TABLE audit_log");
    }
    CHECK_THROWS_AS(db.restoreFrom(notBackup), DatabaseError);
    CHECK(countRows(db, "users") == 1);

    std::filesystem::remove(backupFile);
    std::filesystem::remove(notBackup);
}

// ---- CR1 (v1.1.0): a database made by v1.0.0 has no class_extensions table.

namespace {
bool hasTable(Database& db, const std::string& name) {
    Statement st = db.prepare("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name = ?1");
    st.bindText(1, name);
    st.step();
    return st.columnInt(0) == 1;
}

// Makes a file that looks like lms.db from v1.0.0: the same tables, without class_extensions.
void makeVersion100Database(const std::string& path) {
    Database old(path);
    old.exec("DROP TABLE class_extensions");
    old.exec("INSERT INTO users (username, password_hash, full_name, role, person_number) "
             "VALUES ('keep', 'h', 'N', 'STUDENT', '1')");
    REQUIRE_FALSE(hasTable(old, "class_extensions"));
}
}  // namespace

TEST_CASE("CR1: a v1.0.0 database is upgraded when it is opened, and its data is kept", "[integration][data]") {
    std::string file = (std::filesystem::temp_directory_path() / "lms_test_v100.db").string();
    std::filesystem::remove(file);
    makeVersion100Database(file);
    {
        Database db(file);
        CHECK(hasTable(db, "class_extensions"));
        CHECK(countRows(db, "users") == 1);
        CHECK(countRows(db, "class_extensions") == 0);
    }
    {
        Database again(file);  // opening it a second time changes nothing
        CHECK(countRows(again, "users") == 1);
        Statement st = again.prepare("SELECT count(*) FROM sqlite_master WHERE type = 'table'");
        st.step();
        CHECK(st.columnInt(0) == 12);
    }
    std::filesystem::remove(file);
}

TEST_CASE("CR1: restoring a v1.0.0 backup gives a database with the new table", "[integration][data]") {
    std::string file = (std::filesystem::temp_directory_path() / "lms_test_v100_backup.db").string();
    std::filesystem::remove(file);
    makeVersion100Database(file);

    Database db(":memory:");
    db.restoreFrom(file);
    CHECK(countRows(db, "users") == 1);
    CHECK(hasTable(db, "class_extensions"));
    std::filesystem::remove(file);
}
