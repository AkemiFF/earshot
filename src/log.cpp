// earshot - minimal logging sink shared by the library internals.
#include "log.hpp"

#include <cstdio>
#include <mutex>

namespace earshot::log {
namespace {

std::mutex g_output_mutex;

const char* level_name(earshot_log_level level) {
    switch (level) {
        case EARSHOT_LOG_ERROR:
            return "error";
        case EARSHOT_LOG_WARN:
            return "warning";
        case EARSHOT_LOG_INFO:
            return "info";
        case EARSHOT_LOG_DEBUG:
            return "debug";
    }
    return "log";
}

}  // namespace

void write(const Sink& sink, earshot_log_level level, const std::string& message) {
    if (message.empty()) {
        return;
    }

    if (sink.callback != nullptr) {
        sink.callback(level, message.c_str(), sink.user_data);
        return;
    }

    // Keep the prefix stable so that callers can filter earshot's own output.
    std::lock_guard<std::mutex> lock(g_output_mutex);
    std::fprintf(stderr, "earshot: %s: %s\n", level_name(level), message.c_str());
}

}  // namespace earshot::log
