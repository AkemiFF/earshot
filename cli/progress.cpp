#include "progress.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "term.hpp"

namespace earshot::cli {
namespace {

constexpr std::size_t kMaxNameWidth = 36;

}  // namespace

bool Progress::interactive_supported(bool quiet) {
    return !quiet && term::stderr_is_tty();
}

Progress::Progress(int total_files, bool quiet)
    : Progress(total_files, quiet, interactive_supported(quiet), term::stderr_colour_enabled()) {}

Progress::Progress(int total_files, bool quiet, bool interactive, bool colour)
    : total_(std::max(total_files, 1)),
      quiet_(quiet),
      interactive_(interactive && !quiet),
      colour_(colour && !quiet) {}

Progress::~Progress() {
    if (interactive_ && active_) {
        clear_line();
    }
}

void Progress::begin(int index, const std::string& name, std::int64_t duration_ms) {
    index_ = std::clamp(index, 1, total_);
    name_ = term::basename(name);
    duration_ms_ = std::max<std::int64_t>(duration_ms, 0);
    percent_ = 0;
    started_ = std::chrono::steady_clock::now();
    last_width_ = 0;
    active_ = true;
    if (interactive_) {
        render(0);
    }
}

void Progress::update(int percent) {
    if (!interactive_ || !active_) {
        return;
    }
    percent_ = std::clamp(percent, 0, 100);
    render(percent_);
}

void Progress::end(bool success, bool cancelled) {
    if (quiet_) {
        active_ = false;
        return;
    }
    if (interactive_) {
        clear_line();
    } else {
        std::cerr << status_line(success, cancelled) << '\n';
    }
    active_ = false;
}

void Progress::flush() {
    std::cerr.flush();
}

void Progress::clear_line() {
    if (last_width_ > 0) {
        std::cerr << '\r' << std::string(last_width_, ' ') << '\r';
        std::cerr.flush();
    }
    last_width_ = 0;
}

double Progress::speed() const {
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
    if (elapsed <= 0.05 || duration_ms_ <= 0) {
        return 0.0;
    }
    const double audio_seconds = static_cast<double>(duration_ms_) / 1000.0;
    const double processed = audio_seconds * static_cast<double>(percent_) / 100.0;
    return processed / elapsed;
}

void Progress::render(int percent) {
    const std::string name = term::truncate(name_, kMaxNameWidth);
    std::ostringstream plain;
    plain << '[' << index_ << '/' << total_ << "] " << name << "  " << std::setw(3) << percent
          << "%  " << term::format_speed(speed());

    const std::string text = plain.str();
    const std::string shown = term::paint(text, term::Colour::Cyan, colour_);

    std::cerr << '\r' << shown;
    if (text.size() < last_width_) {
        std::cerr << std::string(last_width_ - text.size(), ' ');
    }
    std::cerr.flush();
    last_width_ = text.size();
}

std::string Progress::status_line(bool success, bool cancelled) const {
    std::ostringstream out;
    out << '[' << index_ << '/' << total_ << "] " << term::basename(name_) << "  ";
    if (cancelled) {
        out << "cancelled";
    } else if (success) {
        out << "done";
    } else {
        out << "failed";
    }
    return out.str();
}

}  // namespace earshot::cli
