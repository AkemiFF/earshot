// earshot - C++ convenience wrapper around the C API.
#ifndef EARSHOT_EARSHOT_HPP
#define EARSHOT_EARSHOT_HPP

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "earshot/earshot.h"

namespace earshot {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string & what) : std::runtime_error(what) {}
};

struct Options {
    std::string model_path;
    std::string vad_model_path;
    std::string language = "auto";
    int         n_threads = 0;
    int         gpu_device = 0;
    bool        use_gpu = true;
    bool        flash_attn = true;
    bool        translate = false;
    bool        no_context = false;
    bool        single_segment = false;
    bool        vad = true;
    float       vad_threshold = 0.5F;
    int         vad_min_silence_ms = 100;
    int         vad_speech_pad_ms = 30;
    float       vad_max_speech_s = 0.0F;
};

struct Segment {
    int64_t     t0_ms = 0;
    int64_t     t1_ms = 0;
    std::string text;
    float       no_speech_prob = 0.0F;
};

struct Result {
    std::vector<Segment> segments;
    int64_t              audio_duration_ms = 0;
    std::string          language;

    [[nodiscard]] std::string text() const;
};

using Progress = std::function<bool(int percent)>;

class Transcriber {
public:
    explicit Transcriber(const Options & options);
    ~Transcriber();

    Transcriber(const Transcriber &) = delete;
    Transcriber & operator=(const Transcriber &) = delete;

    [[nodiscard]] Result transcribe(const std::string & path, const Progress & progress = {});
    [[nodiscard]] Result transcribe_samples(const std::vector<float> & samples,
                                            const Progress & progress = {});

    [[nodiscard]] static std::string version();
    [[nodiscard]] static std::string backend();

private:
    earshot_context * ctx_ = nullptr;
};

}  // namespace earshot

#endif  // EARSHOT_EARSHOT_HPP
