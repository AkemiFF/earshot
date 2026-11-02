#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DEVICE_IO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#include "miniaudio.h"
#pragma GCC diagnostic pop

#include "audio.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>

namespace {

std::string extension(const std::string & path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return {};
    }
    std::string value = path.substr(dot + 1U);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

void set_error(std::string & error, const std::string & message) {
    error = message;
}

}  // namespace

namespace earshot::detail {

bool decode_audio(const std::string & path, AudioBuffer & output, std::string & error) {
    output = {};
    ma_decoder decoder{};
    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 16000);
    if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS) {
        set_error(error, "cannot open or decode audio file: " + path);
        return false;
    }

    ma_uint64 length = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        set_error(error, "cannot determine audio duration: " + path);
        return false;
    }
    constexpr ma_uint64 max_frames = static_cast<ma_uint64>(8) * 60U * 60U * 16000U;
    if (length > max_frames) {
        ma_decoder_uninit(&decoder);
        set_error(error, "audio file is longer than the supported 8 hour limit: " + path);
        return false;
    }
    if (length > 0U && length <= static_cast<ma_uint64>(std::numeric_limits<std::size_t>::max())) {
        output.samples.reserve(static_cast<std::size_t>(length));
    }

    constexpr ma_uint64 chunk_frames = 16384U;
    std::vector<float> buffer(static_cast<std::size_t>(chunk_frames));
    for (;;) {
        ma_uint64 read = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder, buffer.data(), chunk_frames, &read);
        if (result != MA_SUCCESS && result != MA_AT_END) {
            ma_decoder_uninit(&decoder);
            output = {};
            set_error(error, "failed while decoding audio file: " + path);
            return false;
        }
        output.samples.insert(output.samples.end(), buffer.begin(),
                              buffer.begin() + static_cast<std::ptrdiff_t>(read));
        if (result == MA_AT_END || read == 0U) {
            break;
        }
    }
    ma_decoder_uninit(&decoder);
    output.duration_ms = static_cast<int64_t>(output.samples.size()) * 1000 / 16000;
    return true;
}

bool probe_audio(const std::string & path, earshot_audio_info & output, std::string & error) {
    output = {};
    ma_decoder decoder{};
    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 16000);
    if (ma_decoder_init_file(path.c_str(), &config, &decoder) != MA_SUCCESS) {
        set_error(error, "cannot probe audio file: " + path);
        return false;
    }
    output.sample_rate = 16000;
    output.channels = 1;
    ma_uint64 frames = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        set_error(error, "cannot determine audio duration: " + path);
        return false;
    }
    output.duration_ms = frames > static_cast<ma_uint64>(std::numeric_limits<int64_t>::max())
        ? std::numeric_limits<int64_t>::max()
        : static_cast<int64_t>(frames) * 1000 / 16000;
    const std::string ext = extension(path);
    std::snprintf(output.format, sizeof(output.format), "%s", ext.empty() ? "audio" : ext.c_str());
    ma_decoder_uninit(&decoder);
    return true;
}

}  // namespace earshot::detail
