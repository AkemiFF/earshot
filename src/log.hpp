// earshot - minimal logging sink shared by the library internals.
#ifndef EARSHOT_LOG_HPP
#define EARSHOT_LOG_HPP

#include <string>

#include "earshot/earshot.h"

namespace earshot::log {

// A sink is either the user supplied callback or, when empty, stderr.
struct Sink {
    earshot_log_callback callback = nullptr;
    void* user_data = nullptr;
};

// Routes one message to the sink. The message must not contain a trailing newline.
void write(const Sink& sink, earshot_log_level level, const std::string& message);

inline void error(const Sink& sink, const std::string& message) {
    write(sink, EARSHOT_LOG_ERROR, message);
}

inline void warn(const Sink& sink, const std::string& message) {
    write(sink, EARSHOT_LOG_WARN, message);
}

inline void info(const Sink& sink, const std::string& message) {
    write(sink, EARSHOT_LOG_INFO, message);
}

inline void debug(const Sink& sink, const std::string& message) {
    write(sink, EARSHOT_LOG_DEBUG, message);
}

}  // namespace earshot::log

#endif  // EARSHOT_LOG_HPP
