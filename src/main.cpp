// Moderated LMS - composition root.
// Creates the concrete Data Access classes, passes them into the services and starts the menus.
// Step 1 only checks that the program builds and that the libraries load.
#include <iostream>

#include "data/Toolchain.h"

int main() {
    std::cout << "Moderated LMS\n";
    if (!lms::data::initSodium()) {
        std::cerr << "Could not start the password library (libsodium).\n";
        return 1;
    }
    std::cout << "SQLite " << lms::data::sqliteVersion() << ", libsodium ready.\n";
    return 0;
}
