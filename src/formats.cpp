#include "earshot/formats.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {

std::string timestamp(int64_t milliseconds, char separator) {
    milliseconds = std::max<int64_t>(0, milliseconds);
    const int64_t hours = milliseconds / 3600000;
    const int64_t minutes = (milliseconds / 60000) % 60;
    const int64_t seconds = (milliseconds / 1000) % 60;
    const int64_t millis = milliseconds % 1000;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes
           << ':' << std::setw(2) << seconds << separator << std::setw(3) << millis;
    return output.str();
}

std::string json_escape(const std::string & value) {
    std::ostringstream output;
    for (const char raw_character : value) {
        const unsigned char character = static_cast<unsigned char>(raw_character);
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20U) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<unsigned int>(character) << std::dec;
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    return output.str();
}

}  // namespace

namespace earshot::formats {

std::string timestamp_srt(int64_t milliseconds) { return timestamp(milliseconds, ','); }
std::string timestamp_vtt(int64_t milliseconds) { return timestamp(milliseconds, '.'); }

std::string to_txt(const Result & result, TxtStyle style) {
    std::ostringstream output;
    if (style == TxtStyle::Lines) {
        for (const Segment & segment : result.segments) output << segment.text << '\n';
        return output.str();
    }
    std::string paragraph;
    for (const Segment & segment : result.segments) {
        if (!paragraph.empty() && paragraph.size() + segment.text.size() + 1U > 450U) {
            output << paragraph << "\n\n";
            paragraph.clear();
        }
        if (!paragraph.empty()) paragraph += ' ';
        paragraph += segment.text;
    }
    if (!paragraph.empty()) output << paragraph << '\n';
    return output.str();
}

std::string to_srt(const Result & result) {
    std::ostringstream output;
    int index = 1;
    for (const Segment & segment : result.segments) {
        output << index++ << '\n' << timestamp_srt(segment.t0_ms) << " --> "
               << timestamp_srt(segment.t1_ms) << '\n' << segment.text << "\n\n";
    }
    return output.str();
}

std::string to_vtt(const Result & result) {
    std::ostringstream output;
    output << "WEBVTT\n\n";
    for (const Segment & segment : result.segments) {
        output << timestamp_vtt(segment.t0_ms) << " --> " << timestamp_vtt(segment.t1_ms)
               << '\n' << segment.text << "\n\n";
    }
    return output.str();
}

std::string to_json(const Result & result, const std::string & source_path) {
    std::ostringstream output;
    output << "{\n  \"version\": \"0.1.1\",\n  \"source\": \"" << json_escape(source_path)
           << "\",\n  \"language\": \"" << json_escape(result.language) << "\",\n  \"duration\": "
           << std::fixed << std::setprecision(2) << static_cast<double>(result.audio_duration_ms) / 1000.0
           << ",\n  \"segments\": [\n";
    for (std::size_t index = 0; index < result.segments.size(); ++index) {
        const Segment & segment = result.segments[index];
        output << "    {\"id\": " << index + 1U << ", \"start\": "
               << static_cast<double>(segment.t0_ms) / 1000.0 << ", \"end\": "
               << static_cast<double>(segment.t1_ms) / 1000.0 << ", \"text\": \""
               << json_escape(segment.text) << "\"}";
        if (index + 1U != result.segments.size()) output << ',';
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.str();
}

bool is_supported_format(const std::string & extension) {
    return extension == "txt" || extension == "srt" || extension == "vtt" || extension == "json";
}

}  // namespace earshot::formats
