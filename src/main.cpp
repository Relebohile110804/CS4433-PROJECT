// Moderated LMS - composition root.
// Creates the concrete Data Access classes, passes them into the services and starts the menus.
// The menus are added in Step 12; for now it checks the libraries and opens the database.
#include <exception>
#include <iostream>

#include "data/Database.h"
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
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
