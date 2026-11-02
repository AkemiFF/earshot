/*
 * earshot - local speech-to-text
 *
 * Public C API. The header is C99 compatible and safe to include from C++.
 *
 * Lifecycle:
 *     earshot_options opt = earshot_default_options();
 *     opt.model_path = "/path/to/ggml-large-v3-turbo.bin";
 *
 *     char err[EARSHOT_ERROR_BUFFER_SIZE] = {0};
 *     earshot_context * ctx = earshot_create(&opt, err, sizeof err);
 *     if (ctx == NULL) { fputs(err, stderr); return 1; }
 *
 *     earshot_result * res = earshot_transcribe_file(ctx, "talk.mp3", NULL, NULL, err, sizeof err);
 *     for (int i = 0; i < res->n_segments; ++i) puts(res->segments[i].text);
 *
 *     earshot_result_free(res);
 *     earshot_free(ctx);
 */
#ifndef EARSHOT_EARSHOT_H
#define EARSHOT_EARSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EARSHOT_VERSION_MAJOR 0
#define EARSHOT_VERSION_MINOR 1
#define EARSHOT_VERSION_PATCH 0

/* Size that callers should reserve for a human readable error message. */
#define EARSHOT_ERROR_BUFFER_SIZE 512

typedef enum earshot_log_level {
    EARSHOT_LOG_ERROR = 0,
    EARSHOT_LOG_WARN  = 1,
    EARSHOT_LOG_INFO  = 2,
    EARSHOT_LOG_DEBUG = 3
} earshot_log_level;

typedef void (*earshot_log_callback)(earshot_log_level level, const char * message, void * user_data);

/* Return false to abort a running transcription. */
typedef bool (*earshot_progress_callback)(int percent, void * user_data);

typedef struct earshot_context earshot_context;

typedef struct earshot_options {
    /* Absolute or relative path to a ggml whisper model. Required. */
    const char * model_path;

    /* Path to the Silero VAD ggml model. NULL disables voice activity detection. */
    const char * vad_model_path;

    /* Spoken language ("en", "fr", ...) or "auto"/NULL for detection. */
    const char * language;

    /* Worker threads. <= 0 lets earshot pick a sane default. */
    int n_threads;

    /* CUDA device index, ignored when use_gpu is false. */
    int gpu_device;

    bool use_gpu;
    bool flash_attn;

    /* Translate to English instead of transcribing verbatim. */
    bool translate;

    /* Do not feed previous segments back into the decoder as context. */
    bool no_context;

    /* Force one segment for the whole input. */
    bool single_segment;

    bool  vad;
    float vad_threshold;              /* 0.0 .. 1.0, default 0.5 */
    int   vad_min_silence_ms;         /* default 100 */
    int   vad_speech_pad_ms;          /* default 30  */
    float vad_max_speech_s;           /* 0 disables the hard split */

    earshot_log_callback log_callback;
    void *               log_user_data;
} earshot_options;

typedef struct earshot_segment {
    int64_t t0_ms;
    int64_t t1_ms;
    char *  text;          /* owned by the result, never NULL */
    float   no_speech_prob;
} earshot_segment;

typedef struct earshot_result {
    earshot_segment * segments;
    int               n_segments;
    int64_t           audio_duration_ms;
    /* Detected (or forced) language, ISO 639-1. Empty when unknown. */
    char              language[16];
} earshot_result;

typedef struct earshot_audio_info {
    int     sample_rate;
    int     channels;
    int64_t duration_ms;
    char    format[32];
} earshot_audio_info;

/* Options pre-filled with the recommended defaults. */
earshot_options earshot_default_options(void);

/* Loads the models. Returns NULL on failure and fills `error` when provided. */
earshot_context * earshot_create(const earshot_options * options,
                                 char * error, size_t error_size);

void earshot_free(earshot_context * ctx);

/* Transcribes a file supported by the audio decoder.
 * Returns NULL on failure. Free with earshot_result_free(). */
earshot_result * earshot_transcribe_file(earshot_context * ctx,
                                         const char * path,
                                         earshot_progress_callback progress,
                                         void * progress_user_data,
                                         char * error, size_t error_size);

/* Same, for raw mono float32 samples already resampled to 16 kHz. */
earshot_result * earshot_transcribe_samples(earshot_context * ctx,
                                            const float * samples,
                                            size_t n_samples,
                                            earshot_progress_callback progress,
                                            void * progress_user_data,
                                            char * error, size_t error_size);

void earshot_result_free(earshot_result * result);

/* Reads the container header without decoding the whole file. */
bool earshot_probe_audio(const char * path, earshot_audio_info * info,
                         char * error, size_t error_size);

/* "0.1.2" */
const char * earshot_version(void);

/* Version of the embedded whisper.cpp runtime. */
const char * earshot_backend_version(void);

/* Human readable backend description, e.g. "CUDA (1 device)" or "CPU". */
const char * earshot_backend(void);

int earshot_gpu_device_count(void);

/* NULL terminated list of file extensions the decoder understands, e.g. "mp3". */
const char * const * earshot_supported_formats(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EARSHOT_EARSHOT_H */
