#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <system_error>
#include <vector>

#include "args.hpp"
#include "config.hpp"
#include "earshot/earshot.h"
#include "earshot/earshot.hpp"
#include "models.hpp"
#include "output.hpp"
#include "progress.hpp"
#include "term.hpp"
#include "version.hpp"

namespace {

std::atomic<bool> g_interrupted{false};

extern "C" void handle_sigint(int) {
    g_interrupted.store(true);
}

void error_line(const std::string& message) {
    std::cerr << "earshot: " << message << '\n';
}

void warning(const earshot::cli::Options& options, const std::string& message) {
    if (!options.quiet) {
        std::cerr << "earshot: warning: " << message << '\n';
    }
}

void info(const earshot::cli::Options& options, const std::string& message) {
    if (!options.quiet) {
        std::cerr << "earshot: " << message << '\n';
    }
}

void debug(const earshot::cli::Options& options, const std::string& message) {
    if (options.verbose && !options.quiet) {
        std::cerr << "earshot: debug: " << message << '\n';
    }
}

void print_version() {
    const char* backend = earshot_backend_version();
    std::cout << "earshot " << earshot::cli::CLI_VERSION << " (whisper "
              << (backend != nullptr ? backend : "unknown") << ")\n";
}

void print_recap(std::ostream& out, const std::string& input, const earshot::Result& result,
                 const std::string& model, std::int64_t elapsed_ms,
                 const std::vector<std::string>& written) {
    out << "earshot: " << earshot::cli::term::basename(input) << '\n';
    out << "  duration  " << earshot::cli::term::format_duration(result.audio_duration_ms) << '\n';
    out << "  language  " << (result.language.empty() ? std::string("unknown") : result.language)
        << '\n';
    out << "  model     " << model << '\n';
    out << "  elapsed   " << earshot::cli::term::format_duration(elapsed_ms) << '\n';
    if (!written.empty()) {
        out << "  wrote     " << written.front() << '\n';
        for (std::size_t i = 1; i < written.size(); ++i) {
            out << "            " << written[i] << '\n';
        }
    }
    out.flush();
}

std::int64_t elapsed_ms(std::chrono::steady_clock::time_point start) {
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

}  // namespace

int main(int argc, char** argv) {
    using namespace earshot::cli;

    const ParseResult initial = parse_args(argc, argv);
    if (!initial.ok()) {
        error_line(initial.error);
        return 2;
    }
    if (initial.options.help) {
        std::cout << help_text();
        return 0;
    }
    if (initial.options.version) {
        print_version();
        return 0;
    }
    if (initial.options.list_models) {
        std::cout << list_models(resolve_models_dir(initial.options.models_dir));
        return 0;
    }

    Options options;
    std::vector<std::string> config_warnings;
    if (!initial.options.no_config) {
        const std::string path = initial.options.config_path.empty() ? default_config_path()
                                                                     : initial.options.config_path;
        const ConfigLoadResult loaded = load_config(path);
        config_warnings = loaded.warnings;
        apply_config(loaded.config, options);
    }

    const ParseResult parsed = parse_args(argc, argv, options);
    if (!parsed.ok()) {
        error_line(parsed.error);
        return 2;
    }
    options = parsed.options;

    for (const std::string& message : config_warnings) {
        warning(options, message);
    }

    if (std::signal(SIGINT, handle_sigint) == SIG_ERR) {
        debug(options, "could not install the SIGINT handler");
    }
    g_interrupted.store(false);

    const std::string models_dir = resolve_models_dir(options.models_dir);
    debug(options, "models directory: " + models_dir);
    debug(options, std::string("device: ") + device_name(options.device));

    const ResolveModelResult model = resolve_model(options.model, models_dir, options.quiet);
    for (const std::string& message : model.warnings) {
        warning(options, message);
    }
    if (g_interrupted.load()) {
        error_line("cancelled");
        return 3;
    }
    if (!model.ok()) {
        error_line(model.error);
        return model.usage_error ? 2 : 1;
    }
    if (model.downloaded) {
        info(options, "downloaded " + model.path + " (" + format_size(model.size_bytes) + ")");
    }
    debug(options, "model: " + model.path);

    std::string vad_path;
    if (options.vad) {
        const ResolveModelResult vad = resolve_vad_model(models_dir, options.quiet);
        for (const std::string& message : vad.warnings) {
            warning(options, message);
        }
        if (g_interrupted.load()) {
            error_line("cancelled");
            return 3;
        }
        if (!vad.ok()) {
            error_line(vad.error);
            return 1;
        }
        if (vad.downloaded) {
            info(options, "downloaded " + vad.path + " (" + format_size(vad.size_bytes) + ")");
        }
        vad_path = vad.path;
        debug(options, "VAD model: " + vad_path);
    }

    earshot::Options library_options;
    library_options.model_path = model.path;
    library_options.vad_model_path = vad_path;
    library_options.language = options.language;
    library_options.n_threads = options.threads;
    library_options.gpu_device = options.gpu;
    library_options.use_gpu = options.device != Device::Cpu;
    library_options.translate = options.translate;
    library_options.vad = options.vad;
    library_options.vad_threshold = options.vad_threshold;

    std::unique_ptr<earshot::Transcriber> transcriber;
    try {
        transcriber = std::make_unique<earshot::Transcriber>(library_options);
    } catch (const std::exception& error) {
        error_line(error.what());
        return 1;
    }

    const auto run_started = std::chrono::steady_clock::now();
    Progress progress(static_cast<int>(options.inputs.size()), options.quiet);
    std::size_t failed = 0;
    bool wrote_to_stdout = false;

    for (std::size_t index = 0; index < options.inputs.size(); ++index) {
        const std::string& input = options.inputs[index];
        if (g_interrupted.load()) {
            error_line("cancelled");
            progress.flush();
            return 3;
        }

        std::int64_t duration_ms = 0;
        earshot_audio_info audio{};
        char probe_error[EARSHOT_ERROR_BUFFER_SIZE] = {'\0'};
        if (earshot_probe_audio(input.c_str(), &audio, probe_error, sizeof(probe_error))) {
            duration_ms = audio.duration_ms;
        }
        progress.begin(static_cast<int>(index + 1), input, duration_ms);

        std::error_code file_error;
        if (!std::filesystem::exists(input, file_error) ||
            !std::filesystem::is_regular_file(input, file_error)) {
            progress.end(false);
            error_line(input + ": cannot read input file");
            ++failed;
            continue;
        }

        const auto file_started = std::chrono::steady_clock::now();
        earshot::Result result;
        try {
            result = transcriber->transcribe(input, [&progress](int percent) {
                progress.update(percent);
                return !g_interrupted.load(std::memory_order_relaxed);
            });
        } catch (const std::exception& error) {
            if (g_interrupted.load()) {
                progress.end(false, true);
                progress.flush();
                error_line("cancelled");
                return 3;
            }
            progress.end(false);
            error_line(input + ": " + error.what());
            ++failed;
            continue;
        }

        if (g_interrupted.load()) {
            progress.end(false, true);
            progress.flush();
            error_line("cancelled");
            return 3;
        }

        // The C API has no seek option, so --from/--duration are applied to the
        // returned segments and their timestamps are rebased to zero.
        const earshot::Result clipped = clip_result(result, options.from_ms, options.duration_ms);
        const OutputPlan plan = plan_outputs(options, input, options.inputs.size());
        if (!plan.ok()) {
            progress.end(false);
            error_line(input + ": " + plan.error);
            ++failed;
            continue;
        }

        bool file_failed = false;
        std::vector<std::string> written;
        for (const OutputTarget& target : plan.targets) {
            const std::string content = render(clipped, target.format, options.paragraphs, input);
            std::string write_error;
            if (!write_output(target, content, options.force, write_error)) {
                error_line(input + ": " + write_error);
                file_failed = true;
                continue;
            }
            if (target.to_stdout) {
                wrote_to_stdout = true;
                if (std::find(written.begin(), written.end(), "stdout") == written.end()) {
                    written.push_back("stdout");
                }
            } else {
                written.push_back(target.path);
            }
        }

        progress.end(!file_failed);
        if (file_failed) {
            ++failed;
            continue;
        }

        if (options.inputs.size() == 1 && !options.quiet) {
            std::ostream& out = wrote_to_stdout ? std::cerr : std::cout;
            print_recap(out, input, clipped, model.display_name, elapsed_ms(file_started), written);
        }
    }

    progress.flush();
    if (g_interrupted.load()) {
        error_line("cancelled");
        return 3;
    }

    const std::size_t total = options.inputs.size();
    const std::size_t succeeded = total - failed;
    if (total > 1 && !options.quiet) {
        std::ostream& out = wrote_to_stdout ? std::cerr : std::cout;
        out << "earshot: processed " << term::plural(total, "file") << ": " << succeeded
            << " succeeded";
        if (failed > 0) {
            out << ", " << failed << " failed";
        }
        out << " in " << term::format_duration(elapsed_ms(run_started)) << '\n';
        out.flush();
    }

    return failed == 0 ? 0 : 1;
}
