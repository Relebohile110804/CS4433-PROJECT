// Turning the echo of typed keys off and on, so a password is not shown on the screen (R13).
// This is the only platform-specific code in the program.
#include "presentation/ConsoleIO.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace lms::presentation {

bool terminalEcho(bool on) {
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (input == INVALID_HANDLE_VALUE || !GetConsoleMode(input, &mode)) return false;  // not a console
    DWORD wanted = on ? (mode | ENABLE_ECHO_INPUT) : (mode & ~static_cast<DWORD>(ENABLE_ECHO_INPUT));
    return SetConsoleMode(input, wanted) != 0;
#else
    if (!isatty(STDIN_FILENO)) return false;  // input from a file or pipe
    termios settings{};
    if (tcgetattr(STDIN_FILENO, &settings) != 0) return false;
    if (on)
        settings.c_lflag |= ECHO;
    else
        settings.c_lflag &= ~static_cast<tcflag_t>(ECHO);
    return tcsetattr(STDIN_FILENO, TCSANOW, &settings) == 0;
#endif
}

}  // namespace lms::presentation
