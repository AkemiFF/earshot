#include "term.hpp"

#include <unistd.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace earshot::cli::term {
namespace {

const char* environment(const char* name) {
    return std::getenv(name);
}

}  // namespace

bool is_tty(std::FILE* stream) {
    if (stream == nullptr) {
        return false;
    }
    return ::isatty(::fileno(stream)) == 1;
}

bool stdout_is_tty() {
    return is_tty(stdout);
}

bool stderr_is_tty() {
    return is_tty(stderr);
}

bool colour_enabled(std::FILE* stream) {
    if (!is_tty(stream)) {
        return false;
    }
    if (environment("NO_COLOR") != nullptr) {
        return false;
    }
    const char* term = environment("TERM");
    if (term != nullptr && std::strcmp(term, "dumb") == 0) {
        return false;
    }
    return true;
}

bool stdout_colour_enabled() {
    return colour_enabled(stdout);
}

bool stderr_colour_enabled() {
    return colour_enabled(stderr);
}

const char* ansi(Colour colour) {
    switch (colour) {
        case Colour::Reset:
            return "\x1b[0m";
        case Colour::Bold:
            return "\x1b[1m";
        case Colour::Dim:
            return "\x1b[2m";
        case Colour::Red:
            return "\x1b[31m";
        case Colour::Green:
            return "\x1b[32m";
        case Colour::Yellow:
            return "\x1b[33m";
        case Colour::Cyan:
            return "\x1b[36m";
    }
    return "";
}

std::string paint(const std::string& text, Colour colour, bool enabled) {
    if (!enabled || text.empty()) {
        return text;
    }
    std::string out;
    out.reserve(text.size() + 12);
    out += ansi(colour);
    out += text;
    out += ansi(Colour::Reset);
    return out;
}

std::string plural_word(std::size_t count, const std::string& singular,
                        const std::string& plural_form) {
    if (count == 1) {
        return singular;
    }
    if (!plural_form.empty()) {
        return plural_form;
    }
    return singular + "s";
}

std::string plural(std::size_t count, const std::string& singular, const std::string& plural_form) {
    return std::to_string(count) + " " + plural_word(count, singular, plural_form);
}

std::string format_duration(std::int64_t milliseconds) {
    std::ostringstream out;
    if (milliseconds < 0) {
        milliseconds = 0;
    }
    if (milliseconds < 1000) {
        const double seconds = static_cast<double>(milliseconds) / 1000.0;
        out << std::fixed << std::setprecision(1) << seconds << " s";
        return out.str();
    }
    if (milliseconds < 60000) {
        const double seconds = static_cast<double>(milliseconds) / 1000.0;
        out << std::fixed << std::setprecision(1) << seconds << " s";
        return out.str();
    }

    const std::int64_t total_seconds = milliseconds / 1000;
    const std::int64_t seconds = total_seconds % 60;
    const std::int64_t total_minutes = total_seconds / 60;
    if (total_minutes < 60) {
        out << total_minutes << "m " << std::setw(2) << std::setfill('0') << seconds << "s";
        return out.str();
    }
    const std::int64_t hours = total_minutes / 60;
    const std::int64_t minutes = total_minutes % 60;
    out << hours << "h " << std::setw(2) << std::setfill('0') << minutes << "m " << std::setw(2)
        << std::setfill('0') << seconds << "s";
    return out.str();
}

std::string format_speed(double speed) {
    if (!std::isfinite(speed) || speed < 0.0) {
        speed = 0.0;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << speed << "x";
    return out.str();
}

std::string basename(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::string truncate(const std::string& text, std::size_t width) {
    if (text.size() <= width) {
        return text;
    }
    if (width <= 3) {
        return std::string(width, '.');
    }
    return text.substr(0, width - 3) + "...";
}

}  // namespace earshot::cli::term
