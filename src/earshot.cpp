#include "earshot/earshot.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <thread>

#include "audio.hpp"
#include "whisper.h"

namespace {

void write_error(char * buffer, std::size_t size, const std::string & message) {
    if (buffer == nullptr || size == 0U) {
        return;
    }
    const std::size_t count = std::min(size - 1U, message.size());
    std::memcpy(buffer, message.data(), count);
    buffer[count] = '\0';
}

bool blank(const char * text) {
    if (text == nullptr) {
        return true;
    }
    for (const char raw_character : std::string(text)) {
        const unsigned char character = static_cast<unsigned char>(raw_character);
        if (!std::isspace(character)) {
            return false;
        }
    }
    return true;
}

char * copy_text(const char * source) {
    if (source == nullptr) {
        return nullptr;
    }
    const std::size_t length = std::strlen(source);
    char * result = static_cast<char *>(std::malloc(length + 1U));
    if (result == nullptr) {
        return nullptr;
    }
    std::memcpy(result, source, length + 1U);
    return result;
}

struct ProgressState {
    earshot_progress_callback callback = nullptr;
    void * user_data = nullptr;
    std::atomic<bool> cancelled{false};
};

void progress_trampoline(struct whisper_context *, struct whisper_state *, int progress,
                         void * user_data) {
    auto * state = static_cast<ProgressState *>(user_data);
    if (state == nullptr || state->callback == nullptr || state->cancelled.load()) {
        return;
    }
    if (!state->callback(progress, state->user_data)) {
        state->cancelled.store(true);
    }
}

}  // namespace

struct earshot_context {
    whisper_context * whisper = nullptr;
    earshot_options options{};
    std::string model_path;
    std::string vad_model_path;
    std::string language;
    std::mutex mutex;
};

namespace {

earshot_result * make_result(const whisper_context * context, int64_t duration,
                             const std::string & language, std::string & error) {
    const int total = whisper_full_n_segments(const_cast<whisper_context *>(context));
    int count = 0;
    for (int index = 0; index < total; ++index) {
        if (!blank(whisper_full_get_segment_text(const_cast<whisper_context *>(context), index))) {
            ++count;
        }
    }
    auto * result = static_cast<earshot_result *>(std::calloc(1U, sizeof(earshot_result)));
    if (result == nullptr) {
        error = "out of memory while allocating transcription result";
        return nullptr;
    }
    result->audio_duration_ms = duration;
    std::snprintf(result->language, sizeof(result->language), "%s", language.c_str());
    result->n_segments = count;
    if (count > 0) {
        result->segments = static_cast<earshot_segment *>(
            std::calloc(static_cast<std::size_t>(count), sizeof(earshot_segment)));
        if (result->segments == nullptr) {
            earshot_result_free(result);
            error = "out of memory while allocating transcription segments";
            return nullptr;
        }
    }
    int output_index = 0;
    for (int index = 0; index < total; ++index) {
        const char * text = whisper_full_get_segment_text(
            const_cast<whisper_context *>(context), index);
        if (blank(text)) {
            continue;
        }
        while (*text == ' ') {
            ++text;
        }
        result->segments[output_index].text = copy_text(text);
        if (result->segments[output_index].text == nullptr) {
            earshot_result_free(result);
            error = "out of memory while copying transcription text";
            return nullptr;
        }
        result->segments[output_index].t0_ms = whisper_full_get_segment_t0(
            const_cast<whisper_context *>(context), index) * 10;
        result->segments[output_index].t1_ms = whisper_full_get_segment_t1(
            const_cast<whisper_context *>(context), index) * 10;
        result->segments[output_index].no_speech_prob = whisper_full_get_segment_no_speech_prob(
            const_cast<whisper_context *>(context), index);
        ++output_index;
    }
    return result;
}

bool run_transcription(earshot_context * context, const float * samples, std::size_t count,
                       int64_t duration, earshot_progress_callback progress, void * progress_data,
                       earshot_result ** output, std::string & error) {
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        error = "audio sample count exceeds whisper.cpp limits";
        return false;
    }
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.n_threads = context->options.n_threads > 0
        ? context->options.n_threads
        : static_cast<int>(std::max(1U, std::thread::hardware_concurrency()));
    params.translate = context->options.translate;
    params.no_context = context->options.no_context;
    params.single_segment = context->options.single_segment;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.language = context->language.c_str();
    params.detect_language = context->language == "auto";

    ProgressState progress_state{progress, progress_data};
    if (progress != nullptr) {
        params.progress_callback = progress_trampoline;
        params.progress_callback_user_data = &progress_state;
    }
    if (context->options.vad && !context->vad_model_path.empty()) {
        params.vad = true;
        params.vad_model_path = context->vad_model_path.c_str();
        params.vad_params = whisper_vad_default_params();
        params.vad_params.threshold = context->options.vad_threshold;
        params.vad_params.min_silence_duration_ms = context->options.vad_min_silence_ms;
        params.vad_params.speech_pad_ms = context->options.vad_speech_pad_ms;
        if (context->options.vad_max_speech_s > 0.0F) {
            params.vad_params.max_speech_duration_s = context->options.vad_max_speech_s;
        }
    }
    if (whisper_full(context->whisper, params, samples, static_cast<int>(count)) != 0) {
        error = "whisper.cpp failed to transcribe the audio";
        return false;
    }
    if (progress_state.cancelled.load()) {
        error = "transcription cancelled";
        return false;
    }
    std::string detected = context->language;
    if (params.detect_language) {
        const int language_id = whisper_full_lang_id(context->whisper);
        const char * language = whisper_lang_str(language_id);
        detected = language == nullptr ? "" : language;
    }
    *output = make_result(context->whisper, duration, detected, error);
    return *output != nullptr;
}

}  // namespace

extern "C" {

earshot_options earshot_default_options(void) {
    earshot_options options{};
    options.language = "auto";
    options.use_gpu = true;
    options.flash_attn = true;
    options.vad = true;
    options.vad_threshold = 0.5F;
    options.vad_min_silence_ms = 100;
    options.vad_speech_pad_ms = 30;
    return options;
}

earshot_context * earshot_create(const earshot_options * options, char * error, std::size_t error_size) {
    const earshot_options selected = options == nullptr ? earshot_default_options() : *options;
    if (selected.model_path == nullptr || selected.model_path[0] == '\0') {
        write_error(error, error_size, "a whisper model path is required");
        return nullptr;
    }
    if (!std::filesystem::is_regular_file(selected.model_path)) {
        write_error(error, error_size, "whisper model does not exist: " + std::string(selected.model_path));
        return nullptr;
    }
    auto * context = new (std::nothrow) earshot_context();
    if (context == nullptr) {
        write_error(error, error_size, "out of memory while creating earshot context");
        return nullptr;
    }
    context->options = selected;
    context->model_path = selected.model_path;
    context->language = selected.language == nullptr ? "auto" : selected.language;
    if (selected.vad_model_path != nullptr) {
        context->vad_model_path = selected.vad_model_path;
    }
    whisper_context_params params = whisper_context_default_params();
    params.use_gpu = selected.use_gpu;
    params.flash_attn = selected.flash_attn;
    params.gpu_device = selected.gpu_device;
    context->whisper = whisper_init_from_file_with_params(context->model_path.c_str(), params);
    if (context->whisper == nullptr) {
        const std::string failed_model = context->model_path;
        delete context;
        write_error(error, error_size, "failed to load whisper model: " + failed_model);
        return nullptr;
    }
    return context;
}

void earshot_free(earshot_context * context) {
    if (context == nullptr) return;
    whisper_free(context->whisper);
    delete context;
}

earshot_result * earshot_transcribe_samples(earshot_context * context, const float * samples,
                                            std::size_t count, earshot_progress_callback progress,
                                            void * progress_data, char * error, std::size_t error_size) {
    if (context == nullptr || samples == nullptr || count == 0U) {
        write_error(error, error_size, "audio samples are required");
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(context->mutex);
    std::string message;
    earshot_result * result = nullptr;
    if (!run_transcription(context, samples, count, static_cast<int64_t>(count) * 1000 / 16000,
                           progress, progress_data, &result, message)) {
        write_error(error, error_size, message);
        return nullptr;
    }
    return result;
}

earshot_result * earshot_transcribe_file(earshot_context * context, const char * path,
                                         earshot_progress_callback progress, void * progress_data,
                                         char * error, std::size_t error_size) {
    if (context == nullptr || path == nullptr || path[0] == '\0') {
        write_error(error, error_size, "an audio file path is required");
        return nullptr;
    }
    earshot::detail::AudioBuffer audio;
    std::string message;
    if (!earshot::detail::decode_audio(path, audio, message)) {
        write_error(error, error_size, message);
        return nullptr;
    }
    return earshot_transcribe_samples(context, audio.samples.data(), audio.samples.size(), progress,
                                      progress_data, error, error_size);
}

void earshot_result_free(earshot_result * result) {
    if (result == nullptr) return;
    for (int index = 0; index < result->n_segments; ++index) {
        std::free(result->segments[index].text);
    }
    std::free(result->segments);
    std::free(result);
}

bool earshot_probe_audio(const char * path, earshot_audio_info * info, char * error,
                         std::size_t error_size) {
    if (path == nullptr || info == nullptr) {
        write_error(error, error_size, "an audio path and output structure are required");
        return false;
    }
    std::string message;
    if (!earshot::detail::probe_audio(path, *info, message)) {
        write_error(error, error_size, message);
        return false;
    }
    return true;
}

const char * earshot_version(void) { return "0.1.0"; }
const char * earshot_backend_version(void) { return whisper_version(); }
const char * earshot_backend(void) {
#ifdef EARSHOT_HAVE_CUDA
    return "CUDA";
#else
    return "CPU";
#endif
}
int earshot_gpu_device_count(void) {
#ifdef EARSHOT_HAVE_CUDA
    return 1;
#else
    return 0;
#endif
}
const char * const * earshot_supported_formats(void) {
    static const char * formats[] = {"mp3", "wav", "flac", "ogg", "opus", nullptr};
    return formats;
}

}  // extern "C"
