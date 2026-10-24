// Moderated LMS - composition root.
// Creates the concrete Data Access classes, passes them into the services and starts the menus.
// The menus are added in Step 12; for now it checks the libraries, the database and the hasher.
#include <chrono>
#include <exception>
#include <iostream>

#include "data/Database.h"
#include "data/SodiumPasswordHasher.h"
#include "data/Toolchain.h"

int main() {
    std::cout << "Moderated LMS\n";
    if (!lms::data::initSodium()) {
        std::cerr << "Could not start the password library (libsodium).\n";
        return 1;
    }
    std::cout << "SQLite " << lms::data::sqliteVersion() << ", libsodium ready.\n";
    try {
        lms::data::Database db("lms.db");
        std::cout << "Database ready: " << db.getPath() << "\n";

        // R11 check: how long one login's password check takes on this computer (NFR-01: under 2 s).
        lms::data::SodiumPasswordHasher hasher;
        auto start = std::chrono::steady_clock::now();
        hasher.verify(hasher.hash("timing-check"), "timing-check");
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        std::cout << "Password hash + check: " << ms.count() << " ms\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
