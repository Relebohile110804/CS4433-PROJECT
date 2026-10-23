#pragma once
#include <string>

namespace lms::data {

// Start-up checks for the external libraries (risk R6).
// Returns the SQLite version string, for example "3.45.1".
std::string sqliteVersion();

// Initialises libsodium. Must be called once before any hashing; returns false if it fails.
bool initSodium();

}  // namespace lms::data
