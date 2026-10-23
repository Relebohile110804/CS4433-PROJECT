#include "doctest/doctest.h"

#include <fstream>
#include <sstream>

#include <sqlite3.h>

#include "data/Toolchain.h"

TEST_CASE("libsodium initialises") {
    CHECK(lms::data::initSodium());
}

TEST_CASE("SQLite is version 3") {
    CHECK(lms::data::sqliteVersion().rfind("3.", 0) == 0);
}

TEST_CASE("schema.sql loads into an empty database") {
    std::ifstream in(LMS_SCHEMA_PATH);
    REQUIRE(in.good());
    std::stringstream sql;
    sql << in.rdbuf();

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(":memory:", &db) == SQLITE_OK);
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, &err);
    INFO((err ? err : ""));
    CHECK(rc == SQLITE_OK);
    sqlite3_free(err);

    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(db, "SELECT count(*) FROM sqlite_master WHERE type = 'table'", -1, &st, nullptr);
    REQUIRE(sqlite3_step(st) == SQLITE_ROW);
    CHECK(sqlite3_column_int(st, 0) == 11);
    sqlite3_finalize(st);
    sqlite3_close(db);
}
