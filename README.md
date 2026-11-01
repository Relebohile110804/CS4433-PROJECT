# Moderated LMS

CS4433 individual project (student 202321244). C++17 console application with SQLite and libsodium,
built as a closed layered architecture (see the Week Three design document).

## Folder layout

| Folder | Layer / purpose |
|---|---|
| `src/presentation/` | Presentation: Console I/O and the role menus |
| `src/business/domain/` | Business Logic: domain model (User, Course, Assignment, Submission, ...) |
| `src/business/services/` | Business Logic: services, interfaces and the late penalty policy |
| `src/data/` | Data Access: SQLite repositories, transactions, file store, password hasher, clock |
| `src/main.cpp` | Composition root: creates the concrete classes and wires them together |
| `database/schema.sql` | Database schema (Week Three, Section 4.3) |
| `tests/` | Automated tests (Catch2), tagged [unit], [integration], [system] or [acceptance] |
| `third_party/catch2/` | Catch2 v3.7.1, amalgamated single-file build |

Each layer is its own library in CMake, and a layer only links the one below it, so a call that skips a layer fails to build.

## Building

### Windows with VS Code (main way to build)

One-time setup:

1. Install the VS Code extensions **C/C++** and **CMake Tools** (both by Microsoft).
2. Install MSYS2 from https://www.msys2.org (keep the default folder `C:\msys64`).
3. Open the **MSYS2 UCRT64** terminal from the Start menu and run:
   ```
   pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-libsodium
   ```
   Press Enter to accept all, and Y to confirm.
4. Add `C:\msys64\ucrt64\bin` to your Windows **Path** (Start > "Edit the system environment variables" > Environment Variables > Path > New).
5. Close and reopen VS Code.

Opening and running the project:

1. **File > Open Folder** and pick the `lms` folder that has `CMakeLists.txt` directly inside it.
2. When CMake Tools asks for a configure preset, pick **Windows Debug (VS Code + MSYS2 GCC)**. You can change it later with Ctrl+Shift+P > "CMake: Select Configure Preset".
3. Click **Build** in the blue status bar at the bottom.
4. Click the **play** button in the status bar to run `lms`. To run the tests, Ctrl+Shift+P > "CMake: Run Tests".

### Windows with Visual Studio 2022 (alternative)

One-time setup:

1. In the Visual Studio Installer, make sure the **Desktop development with C++** workload is installed. It includes CMake and the C++ compiler.
2. Install vcpkg, which downloads SQLite and libsodium for you. Open a normal Command Prompt and run:
   ```
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   setx VCPKG_ROOT C:\vcpkg
   ```
3. Close and reopen Visual Studio so it sees `VCPKG_ROOT`.

Opening and running the project:

1. In Visual Studio choose **File > Open > Folder** and pick this `lms` folder (there is no .sln file; Visual Studio reads `CMakeLists.txt` directly).
2. In the toolbar, pick the **Windows x64 Debug** configuration. The first time, vcpkg builds SQLite and libsodium from `vcpkg.json`, which takes a few minutes.
3. Pick **lms.exe** as the startup item and press **F5** to run it, or **Build > Build All**.
4. To run the tests, open **Test > Test Explorer** and click **Run All**, or pick **lms_tests.exe** as the startup item.

### Linux (Ubuntu 22.04 or later)

```
sudo apt-get install build-essential cmake libsqlite3-dev libsodium-dev gcovr
cmake --preset linux-debug
cmake --build --preset linux-debug
./build/linux-debug/lms
```

## Tests and coverage

```
ctest --preset linux-debug
```

The tests use Catch2 and are tagged by V-Model level, so one level can be run on its own:

```
lms_tests "[unit]"
lms_tests "[integration]"
lms_tests "[system]"
lms_tests "[acceptance]"
```

The system tests (ST-xx) and acceptance tests (AT-xx) in tests/system run the real menus with a real database file in a temporary folder. ST-45 (NFR-08) kills a child process, so it runs on Linux only and is skipped on Windows.

Timing results (Argon2id and the full-size performance test) are printed as Catch2 "warning" messages; they are information, not failures.

When the program meets a database or file error it tells the user and also writes the error, with the time in UTC, to lms-errors.log next to lms.db.

Business-layer coverage (NFR-12 target: 70% of lines) is measured on Linux with gcov, which is also what CI runs:

```
cmake -S . -B build-cov -DLMS_COVERAGE=ON
cmake --build build-cov
ctest --test-dir build-cov
gcovr -r . --filter src/business/ build-cov
```

CI runs the same steps on every push, fails if business-layer coverage drops below 70%, and saves the JUnit test results and an HTML coverage report as a build artifact.
