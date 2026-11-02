#include "earshot/earshot.hpp"

#include <cstring>
#include <memory>
#include <utility>

namespace {

struct CallbackState {
    const earshot::Progress * callback = nullptr;
};

bool progress_bridge(int percent, void * data) {
    const auto * state = static_cast<const CallbackState *>(data);
    return state == nullptr || state->callback == nullptr || !*state->callback ||
           (*state->callback)(percent);
}

}  // namespace

namespace earshot {

Transcriber::Transcriber(const Options & options) {
    earshot_options native = earshot_default_options();
    native.model_path = options.model_path.c_str();
    native.vad_model_path = options.vad_model_path.empty() ? nullptr : options.vad_model_path.c_str();
    native.language = options.language.c_str();
    native.n_threads = options.n_threads;
    native.gpu_device = options.gpu_device;
    native.use_gpu = options.use_gpu;
    native.flash_attn = options.flash_attn;
    native.translate = options.translate;
    native.no_context = options.no_context;
    native.single_segment = options.single_segment;
    native.vad = options.vad;
    native.vad_threshold = options.vad_threshold;
    native.vad_min_silence_ms = options.vad_min_silence_ms;
    native.vad_speech_pad_ms = options.vad_speech_pad_ms;
    native.vad_max_speech_s = options.vad_max_speech_s;
    char error[EARSHOT_ERROR_BUFFER_SIZE] = {};
    ctx_ = earshot_create(&native, error, sizeof(error));
    if (ctx_ == nullptr) {
        throw Error(error);
    }
}

Transcriber::~Transcriber() { earshot_free(ctx_); }

Result Transcriber::transcribe(const std::string & path, const Progress & progress) {
    char error[EARSHOT_ERROR_BUFFER_SIZE] = {};
    CallbackState state{&progress};
    earshot_result * native = earshot_transcribe_file(
        ctx_, path.c_str(), progress ? progress_bridge : nullptr, &state, error, sizeof(error));
    if (native == nullptr) {
        throw Error(error);
    }
    std::unique_ptr<earshot_result, decltype(&earshot_result_free)> holder(native, earshot_result_free);
    Result result;
    result.audio_duration_ms = native->audio_duration_ms;
    result.language = native->language;
    result.segments.reserve(static_cast<std::size_t>(native->n_segments));
    for (int index = 0; index < native->n_segments; ++index) {
        result.segments.push_back({native->segments[index].t0_ms, native->segments[index].t1_ms,
                                   native->segments[index].text, native->segments[index].no_speech_prob});
    }
    return result;
}

Result Transcriber::transcribe_samples(const std::vector<float> & samples, const Progress & progress) {
    char error[EARSHOT_ERROR_BUFFER_SIZE] = {};
    CallbackState state{&progress};
    earshot_result * native = earshot_transcribe_samples(
        ctx_, samples.data(), samples.size(), progress ? progress_bridge : nullptr, &state,
        error, sizeof(error));
    if (native == nullptr) {
        throw Error(error);
    }
    std::unique_ptr<earshot_result, decltype(&earshot_result_free)> holder(native, earshot_result_free);
    Result result;
    result.audio_duration_ms = native->audio_duration_ms;
    result.language = native->language;
    for (int index = 0; index < native->n_segments; ++index) {
        result.segments.push_back({native->segments[index].t0_ms, native->segments[index].t1_ms,
                                   native->segments[index].text, native->segments[index].no_speech_prob});
    }
    return result;
}

std::string Result::text() const {
    std::string output;
    for (const Segment & segment : segments) {
        if (!output.empty()) output += ' ';
        output += segment.text;
    }
    return output;
}

std::string Transcriber::version() { return earshot_version(); }
std::string Transcriber::backend() { return earshot_backend(); }

}  // namespace earshot
