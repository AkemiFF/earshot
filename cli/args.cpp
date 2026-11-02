#include "args.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace earshot::cli {
namespace {

std::string trim(const std::string& text) {
    std::size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])) != 0) {
        ++first;
    }
    std::size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) {
        --last;
    }
    return text.substr(first, last - first);
}

std::string lowercase_ascii(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

bool ends_with(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

ParseResult failure(const std::string& message) {
    ParseResult result;
    result.error = message + "\ntry 'earshot --help'";
    return result;
}

bool parse_int64(const std::string& text, std::int64_t& out) {
    if (text.empty()) {
        return false;
    }
    std::size_t first = 0;
    if (text[0] == '+') {
        first = 1;
    }
    if (first >= text.size()) {
        return false;
    }
    const char* begin = text.data() + first;
    const char* end = text.data() + text.size();
    std::int64_t value = 0;
    const std::from_chars_result parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc() || parsed.ptr != end) {
        return false;
    }
    out = value;
    return true;
}

bool parse_float(const std::string& text, float& out) {
    if (text.empty()) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || end == nullptr || *end != '\0' || errno == ERANGE ||
        !std::isfinite(value)) {
        return false;
    }
    out = value;
    return true;
}

enum class ValueOption {
    Model,
    Language,
    Output,
    Format,
    VadThreshold,
    From,
    Duration,
    Device,
    Gpu,
    Threads,
    Config,
    ModelsDir,
};

bool parse_value_option(const std::string& label, ValueOption option, const std::string& value,
                        Options& options, std::string& error) {
    switch (option) {
        case ValueOption::Model:
            if (value.empty()) {
                error = "option '" + label + "' requires a non-empty value";
                return false;
            }
            options.model = value;
            return true;
        case ValueOption::Language:
            if (value.empty()) {
                error = "option '" + label + "' requires a non-empty value";
                return false;
            }
            options.language = value;
            return true;
        case ValueOption::Output:
            if (value.empty()) {
                error = "option '" + label + "' requires a non-empty value";
                return false;
            }
            options.output = value;
            return true;
        case ValueOption::Format: {
            std::vector<Format> formats;
            std::string detail;
            if (!parse_format_list(value, formats, detail)) {
                error = "invalid value for '" + label + "': " + detail;
                return false;
            }
            options.formats = formats;
            return true;
        }
        case ValueOption::VadThreshold: {
            float threshold = 0.0F;
            if (!parse_float(value, threshold) || threshold < 0.0F || threshold > 1.0F) {
                error = "invalid value for '" + label + "': expected a number between 0.0 and 1.0";
                return false;
            }
            options.vad_threshold = threshold;
            return true;
        }
        case ValueOption::From:
        case ValueOption::Duration: {
            std::int64_t milliseconds = 0;
            if (!parse_int64(value, milliseconds) || milliseconds < 0) {
                error = "invalid value for '" + label + "': expected a non-negative integer";
                return false;
            }
            if (option == ValueOption::From) {
                options.from_ms = milliseconds;
            } else {
                options.duration_ms = milliseconds;
            }
            return true;
        }
        case ValueOption::Device: {
            Device device = Device::Auto;
            if (!parse_device(value, device)) {
                error = "invalid value for '" + label + "': expected auto, cpu or cuda";
                return false;
            }
            options.device = device;
            return true;
        }
        case ValueOption::Gpu:
        case ValueOption::Threads: {
            std::int64_t number = 0;
            if (!parse_int64(value, number) || number < 0 ||
                number > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
                error = "invalid value for '" + label + "': expected a non-negative integer";
                return false;
            }
            if (option == ValueOption::Gpu) {
                options.gpu = static_cast<int>(number);
            } else {
                options.threads = static_cast<int>(number);
            }
            return true;
        }
        case ValueOption::Config:
            if (value.empty()) {
                error = "option '" + label + "' requires a non-empty value";
                return false;
            }
            options.config_path = value;
            return true;
        case ValueOption::ModelsDir:
            if (value.empty()) {
                error = "option '" + label + "' requires a non-empty value";
                return false;
            }
            options.models_dir = value;
            return true;
    }
    error = "unknown option '" + label + "'";
    return false;
}

bool long_value_option(const std::string& name, ValueOption& option) {
    if (name == "model") {
        option = ValueOption::Model;
    } else if (name == "language") {
        option = ValueOption::Language;
    } else if (name == "output") {
        option = ValueOption::Output;
    } else if (name == "format") {
        option = ValueOption::Format;
    } else if (name == "vad-threshold") {
        option = ValueOption::VadThreshold;
    } else if (name == "from") {
        option = ValueOption::From;
    } else if (name == "duration") {
        option = ValueOption::Duration;
    } else if (name == "device") {
        option = ValueOption::Device;
    } else if (name == "gpu") {
        option = ValueOption::Gpu;
    } else if (name == "threads") {
        option = ValueOption::Threads;
    } else if (name == "config") {
        option = ValueOption::Config;
    } else if (name == "models-dir") {
        option = ValueOption::ModelsDir;
    } else {
        return false;
    }
    return true;
}

ParseResult parse_tokens(const std::vector<std::string>& tokens, const Options& defaults) {
    ParseResult result;
    result.options = defaults;
    Options& options = result.options;

    bool end_of_options = false;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (!end_of_options && token == "--") {
            end_of_options = true;
            continue;
        }
        if (end_of_options || token.empty() || token == "-" || token[0] != '-') {
            options.inputs.push_back(token);
            continue;
        }

        if (token.size() >= 2 && token[0] == '-' && token[1] == '-') {
            const std::size_t equals = token.find('=');
            const std::string name =
                token.substr(2, equals == std::string::npos ? std::string::npos : equals - 2);
            const bool has_inline = equals != std::string::npos;
            const std::string inline_value = has_inline ? token.substr(equals + 1) : std::string();

            bool flag = true;
            if (name == "translate") {
                options.translate = true;
            } else if (name == "vad") {
                options.vad = true;
            } else if (name == "no-vad") {
                options.vad = false;
            } else if (name == "list-models") {
                options.list_models = true;
            } else if (name == "paragraphs") {
                options.paragraphs = true;
            } else if (name == "stdout") {
                options.to_stdout = true;
            } else if (name == "force") {
                options.force = true;
            } else if (name == "quiet") {
                options.quiet = true;
            } else if (name == "verbose") {
                options.verbose = true;
            } else if (name == "config") {
                flag = false;
            } else if (name == "no-config") {
                options.no_config = true;
            } else if (name == "version") {
                options.version = true;
            } else if (name == "help") {
                options.help = true;
            } else {
                ValueOption option = ValueOption::Model;
                if (!long_value_option(name, option)) {
                    return failure("unknown option '--" + name + "'");
                }
                flag = false;
            }

            if (flag) {
                if (has_inline) {
                    return failure("option '--" + name + "' does not take a value");
                }
                continue;
            }

            ValueOption option = ValueOption::Model;
            long_value_option(name, option);

            std::string value;
            if (has_inline) {
                value = inline_value;
            } else if (i + 1 < tokens.size()) {
                value = tokens[++i];
            } else {
                return failure("option '--" + name + "' requires a value");
            }

            std::string detail;
            if (!parse_value_option("--" + name, option, value, options, detail)) {
                return failure(detail);
            }
            continue;
        }

        std::size_t position = 1;
        while (position < token.size()) {
            const char short_name = token[position];
            if (short_name == 'h') {
                options.help = true;
                ++position;
            } else if (short_name == 'q') {
                options.quiet = true;
                ++position;
            } else if (short_name == 'v') {
                options.verbose = true;
                ++position;
            } else if (short_name == 'p') {
                options.paragraphs = true;
                ++position;
            } else if (short_name == 'm' || short_name == 'l' || short_name == 'o' ||
                       short_name == 't') {
                ValueOption option = ValueOption::Model;
                if (short_name == 'm') {
                    option = ValueOption::Model;
                } else if (short_name == 'l') {
                    option = ValueOption::Language;
                } else if (short_name == 'o') {
                    option = ValueOption::Output;
                } else {
                    option = ValueOption::Threads;
                }

                std::string value;
                if (position + 1 < token.size()) {
                    value = token.substr(position + 1);
                    if (!value.empty() && value[0] == '=') {
                        value.erase(0, 1);
                    }
                } else if (i + 1 < tokens.size()) {
                    value = tokens[++i];
                } else {
                    return failure("option '-" + std::string(1, short_name) + "' requires a value");
                }

                std::string detail;
                if (!parse_value_option("-" + std::string(1, short_name), option, value, options,
                                        detail)) {
                    return failure(detail);
                }
                position = token.size();
            } else {
                return failure("unknown option '-" + std::string(1, short_name) + "'");
            }
        }
    }

    if (options.output == "-") {
        options.to_stdout = true;
    }
    if (options.duration_ms > 0 &&
        options.from_ms > std::numeric_limits<std::int64_t>::max() - options.duration_ms) {
        return failure("the --from and --duration values are too large together");
    }
    if (!options.help && !options.version && !options.list_models && options.inputs.empty()) {
        return failure("no input files");
    }
    return result;
}

bool looks_like_program(const std::string& value) {
    if (value == "earshot") {
        return true;
    }
    return ends_with(value, "/earshot") || ends_with(value, "\\earshot");
}

std::vector<std::string> arguments_only(const std::vector<std::string>& args) {
    if (!args.empty() && looks_like_program(args.front())) {
        return std::vector<std::string>(args.begin() + 1, args.end());
    }
    return args;
}

}  // namespace

ParseResult parse_args(int argc, const char* const* argv) {
    return parse_args(argc, argv, Options{});
}

ParseResult parse_args(int argc, const char* const* argv, const Options& defaults) {
    std::vector<std::string> tokens;
    if (argc > 1 && argv != nullptr) {
        tokens.reserve(static_cast<std::size_t>(argc - 1));
        for (int i = 1; i < argc; ++i) {
            tokens.emplace_back(argv[i] != nullptr ? argv[i] : "");
        }
    }
    return parse_tokens(tokens, defaults);
}

ParseResult parse_args(const std::vector<std::string>& args) {
    return parse_args(args, Options{});
}

ParseResult parse_args(const std::vector<std::string>& args, const Options& defaults) {
    return parse_tokens(arguments_only(args), defaults);
}

const char* format_name(Format format) {
    switch (format) {
        case Format::Txt:
            return "txt";
        case Format::Srt:
            return "srt";
        case Format::Vtt:
            return "vtt";
        case Format::Json:
            return "json";
    }
    return "txt";
}

const char* format_extension(Format format) {
    return format_name(format);
}

bool parse_format_list(const std::string& list, std::vector<Format>& out, std::string& error) {
    out.clear();
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = list.find(',', start);
        const std::string token = trim(
            list.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (token.empty()) {
            error = "empty format in list";
            return false;
        }
        const std::string name = lowercase_ascii(token);
        Format format = Format::Txt;
        if (name == "txt") {
            format = Format::Txt;
        } else if (name == "srt") {
            format = Format::Srt;
        } else if (name == "vtt") {
            format = Format::Vtt;
        } else if (name == "json") {
            format = Format::Json;
        } else {
            error = "unknown format '" + token + "' (expected txt, srt, vtt or json)";
            return false;
        }
        if (std::find(out.begin(), out.end(), format) == out.end()) {
            out.push_back(format);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    if (out.empty()) {
        error = "no formats given";
        return false;
    }
    return true;
}

const char* device_name(Device device) {
    switch (device) {
        case Device::Auto:
            return "auto";
        case Device::Cpu:
            return "cpu";
        case Device::Cuda:
            return "cuda";
    }
    return "auto";
}

bool parse_device(const std::string& text, Device& out) {
    const std::string name = lowercase_ascii(text);
    if (name == "auto") {
        out = Device::Auto;
        return true;
    }
    if (name == "cpu") {
        out = Device::Cpu;
        return true;
    }
    if (name == "cuda") {
        out = Device::Cuda;
        return true;
    }
    return false;
}

std::string usage_line() {
    return "Usage: earshot [options] <file>...";
}

std::string help_text() {
    return usage_line() +
           "\n\n"
           "Transcribe audio files locally with whisper.cpp.\n"
           "\n"
           "Model:\n"
           "  -m, --model NAME|PATH   model name from the catalogue or path to a ggml .bin\n"
           "                          (default: large-v3-turbo)\n"
           "      --list-models       list the available models and exit\n"
           "      --models-dir DIR    override the model cache directory\n"
           "\n"
           "Transcription:\n"
           "  -l, --language LANG     ISO 639-1 language code, or \"auto\" (default: auto)\n"
           "      --translate         translate speech to English\n"
           "      --vad               enable voice activity detection (default)\n"
           "      --no-vad            disable voice activity detection\n"
           "      --vad-threshold F   VAD threshold between 0.0 and 1.0 (default: 0.5)\n"
           "      --from MS           start offset in milliseconds (default: 0)\n"
           "      --duration MS       only process this many milliseconds\n"
           "\n"
           "Output:\n"
           "  -o, --output PATH       output file, directory, or \"-\" for stdout\n"
           "      --format LIST       comma separated: txt, srt, vtt, json (default: txt)\n"
           "  -p, --paragraphs        group the txt output into paragraphs\n"
           "      --stdout            write the transcript(s) to stdout\n"
           "      --force             overwrite existing output files\n"
           "\n"
           "Device:\n"
           "      --device BACKEND    auto, cpu or cuda (default: auto)\n"
           "      --gpu N             GPU index (default: 0)\n"
           "  -t, --threads N         worker threads, 0 picks a default (default: 0)\n"
           "\n"
           "General:\n"
           "      --config PATH       config file (default: $XDG_CONFIG_HOME/earshot/config.toml)\n"
           "      --no-config         ignore the config file\n"
           "  -q, --quiet             no progress or information, errors only\n"
           "  -v, --verbose           debug logging\n"
           "      --version           print the version and exit\n"
           "  -h, --help              show this help and exit\n"
           "\n"
           "Exit status: 0 success, 1 runtime failure, 2 usage error, 3 cancelled.\n";
}

}  // namespace earshot::cli
