// NFR-06 and NFR-07 were first checked by reading the code (Week Five, Section 2.3).
// These two tests repeat that inspection automatically on every build, so a later change
// cannot break either rule without the build failing. They read the source files, not the
// running program.
#include "catch2/catch_amalgamated.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// Every .h and .cpp file of the program (not the tests or third-party code).
std::vector<fs::path> programFiles() {
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(fs::path(LMS_SOURCE_DIR) / "src")) {
        std::string ext = entry.path().extension().string();
        if (entry.is_regular_file() && (ext == ".h" || ext == ".cpp")) files.push_back(entry.path());
    }
    return files;
}

// True if any one line of the text matches. Searching line by line keeps std::regex
// from running out of stack on a long file (the Windows stack is only 1 MB).
bool anyLineMatches(const std::string& code, const std::regex& pattern) {
    std::istringstream lines(code);
    std::string line;
    while (std::getline(lines, line))
        if (std::regex_search(line, pattern)) return true;
    return false;
}

// The text between the bracket after "at" and its matching closing bracket.
// Brackets inside string literals are ignored.
std::string callArguments(const std::string& code, std::size_t at) {
    std::size_t open = code.find('(', at);
    int depth = 0;
    bool inString = false;
    for (std::size_t i = open; i < code.size(); ++i) {
        char c = code[i];
        if (inString) {
            if (c == '\\') ++i;
            else if (c == '"') inString = false;
        } else if (c == '"') {
            inString = true;
        } else if (c == '(') {
            ++depth;
        } else if (c == ')' && --depth == 0) {
            return code.substr(open + 1, i - open - 1);
        }
    }
    return code.substr(open + 1);
}

// The arguments with every string literal and all white space taken out.
std::string withoutLiterals(const std::string& args) {
    std::string out;
    bool inString = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        char c = args[i];
        if (inString) {
            if (c == '\\') ++i;
            else if (c == '"') inString = false;
        } else if (c == '"') {
            inString = true;
        } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            out += c;
        }
    }
    return out;
}

}  // namespace

TEST_CASE("ST-50: NFR-07 the program contains no network code and links no network library", "[system]") {
    // Headers and calls that any network code would need, on Windows or Linux.
    const std::regex network(
        R"(#include\s*<(winsock2?\.h|ws2tcpip\.h|wininet\.h|winhttp\.h|sys/socket\.h|netinet/[a-z_]+\.h|arpa/inet\.h|netdb\.h|curl/curl\.h|boost/asio[^>]*)>)"
        R"(|\b(socket|connect|getaddrinfo|gethostbyname|WSAStartup|InternetOpen[AW]?|WinHttpOpen|curl_easy_init)\s*\()");
    std::vector<fs::path> files = programFiles();
    REQUIRE(files.size() > 40);
    for (const fs::path& file : files) {
        std::string code = readFile(file);
        INFO(file.string());
        CHECK_FALSE(anyLineMatches(code, network));
    }

    // The build links only SQLite and libsodium: no socket, HTTP or SSL library.
    std::string cmake = readFile(fs::path(LMS_SOURCE_DIR) / "CMakeLists.txt");
    const std::regex networkLib(R"(\b(ws2_32|wsock32|wininet|winhttp|curl|ssl|crypto|boost_system)\b)",
                                std::regex::icase);
    CHECK_FALSE(anyLineMatches(cmake, networkLib));
}

TEST_CASE("ST-51: NFR-06 all SQL is fixed text with ? parameters; no typed value is joined into SQL", "[system]") {
    // Only these files may use the SQLite library directly. Database.cpp holds Statement,
    // which takes values only through bind calls; Toolchain.cpp only reads the version.
    const std::regex sqliteHeader(R"(#include\s*<sqlite3\.h>)");
    const std::regex printfSql(R"(\bsqlite3_(mprintf|snprintf|vmprintf)\b)");
    // A call that builds SQL: db_.prepare(...), db.prepare(...), exec(...) and so on.
    const std::regex sqlCall(R"(\b(prepare|exec)\s*\()");
    // After the string literals are removed, an argument may only be one of the fixed
    // SELECT texts declared as "const char* const" in the same file, joined with "+".
    const std::regex allowedRest(R"((std::string\(Select[A-Za-z]+\)\+?)*)");

    int callsChecked = 0;
    for (const fs::path& file : programFiles()) {
        std::string name = file.filename().string();
        std::string code = readFile(file);
        INFO(file.string());
        CHECK_FALSE(anyLineMatches(code, printfSql));
        if (name != "Database.cpp" && name != "Toolchain.cpp") CHECK_FALSE(anyLineMatches(code, sqliteHeader));
        if (name == "Database.cpp" || file.extension() != ".cpp") continue;  // see below for Database.cpp

        std::istringstream lines(code);
        std::string line;
        std::size_t lineStart = 0;
        for (; std::getline(lines, line); lineStart += line.size() + 1) {
            std::smatch call;
            if (!std::regex_search(line, call, sqlCall)) continue;
            std::string args = callArguments(code, lineStart + static_cast<std::size_t>(call.position()));
            INFO("call: " << args);
            CHECK(std::regex_match(withoutLiterals(args), allowedRest));
            ++callsChecked;
        }
    }
    CHECK(callsChecked >= 40);  // the repositories really were read (48 calls in v1.1.0)

    // Database.cpp joins only its own savepoint names ("sp" and a counter) and the schema
    // text built into the program; neither can contain anything a user typed.
    std::string db = readFile(fs::path(LMS_SOURCE_DIR) / "src" / "data" / "Database.cpp");
    CHECK(db.find(R"(name_("sp" + std::to_string(db.nextSavepoint_++)))") != std::string::npos);
}
