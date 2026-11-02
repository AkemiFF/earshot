#include <cassert>
#include <string>

#include "earshot/formats.hpp"

int main() {
    earshot::Result result;
    result.language = "en";
    result.audio_duration_ms = 4567;
    result.segments.push_back({120, 1450, "Hello", 0.01F});
    result.segments.push_back({1500, 4567, "world", 0.02F});

    assert(earshot::formats::timestamp_srt(3723500) == "01:02:03,500");
    assert(earshot::formats::timestamp_vtt(3723500) == "01:02:03.500");
    assert(earshot::formats::to_txt(result) == "Hello\nworld\n");
    assert(earshot::formats::to_srt(result).find("00:00:00,120 --> 00:00:01,450") != std::string::npos);
    assert(earshot::formats::to_vtt(result).compare(0, 8, "WEBVTT\n\n") == 0);
    assert(earshot::formats::to_json(result, "a\\b\"c").find("a\\\\b\\\"c") != std::string::npos);
    assert(earshot::formats::is_supported_format("vtt"));
    assert(!earshot::formats::is_supported_format("yaml"));
    return 0;
}
