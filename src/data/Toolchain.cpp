#include "data/Toolchain.h"

#include <sodium.h>
#include <sqlite3.h>

namespace lms::data {

std::string sqliteVersion() { return sqlite3_libversion(); }

bool initSodium() { return sodium_init() >= 0; }  // 1 means it was already initialised

}  // namespace lms::data
