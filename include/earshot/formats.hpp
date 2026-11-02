// earshot - transcript serialisation helpers (C++ only, part of libearshot).
#ifndef EARSHOT_FORMATS_HPP
#define EARSHOT_FORMATS_HPP

#include <cstdint>
#include <string>

#include "earshot/earshot.hpp"

namespace earshot::formats {

enum class TxtStyle {
    Lines,       // one line per segment
    Paragraphs,  // segments regrouped into readable paragraphs
};

// "00:01:02,500" (SRT) and "00:01:02.500" (WebVTT)
[[nodiscard]] std::string timestamp_srt(int64_t milliseconds);
[[nodiscard]] std::string timestamp_vtt(int64_t milliseconds);

[[nodiscard]] std::string to_txt(const Result & result, TxtStyle style = TxtStyle::Lines);
[[nodiscard]] std::string to_srt(const Result & result);
[[nodiscard]] std::string to_vtt(const Result & result);
[[nodiscard]] std::string to_json(const Result & result, const std::string & source_path = {});

// Extensions understood by --format, e.g. {"txt", "srt", "vtt", "json"}.
[[nodiscard]] bool is_supported_format(const std::string & extension);

}  // namespace earshot::formats

#endif  // EARSHOT_FORMATS_HPP
