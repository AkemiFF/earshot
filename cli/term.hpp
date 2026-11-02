#ifndef EARSHOT_CLI_TERM_HPP
#define EARSHOT_CLI_TERM_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace earshot::cli::term {

enum class Colour {
    Reset,
    Bold,
    Dim,
    Red,
    Green,
    Yellow,
    Cyan,
};

// True when `stream` refers to a terminal.
[[nodiscard]] bool is_tty(std::FILE* stream);
[[nodiscard]] bool stdout_is_tty();
[[nodiscard]] bool stderr_is_tty();

// Colour is only enabled for terminals that are not explicitly monochrome.
[[nodiscard]] bool colour_enabled(std::FILE* stream);
[[nodiscard]] bool stdout_colour_enabled();
[[nodiscard]] bool stderr_colour_enabled();

[[nodiscard]] const char* ansi(Colour colour);
[[nodiscard]] std::string paint(const std::string& text, Colour colour, bool enabled);

// "1 file", "2 files". `plural_form` overrides the default "-s" suffix.
[[nodiscard]] std::string plural_word(std::size_t count, const std::string& singular,
                                      const std::string& plural_form = {});
[[nodiscard]] std::string plural(std::size_t count, const std::string& singular,
                                 const std::string& plural_form = {});

// Friendly wall-clock duration: "0.4 s", "4.2 s", "1m 02s", "1h 02m 03s".
[[nodiscard]] std::string format_duration(std::int64_t milliseconds);

// One decimal place and a trailing "x", e.g. "1.2x".
[[nodiscard]] std::string format_speed(double speed);

[[nodiscard]] std::string basename(const std::string& path);
[[nodiscard]] std::string truncate(const std::string& text, std::size_t width);

}  // namespace earshot::cli::term

#endif  // EARSHOT_CLI_TERM_HPP
