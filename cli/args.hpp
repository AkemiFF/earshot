#ifndef EARSHOT_CLI_ARGS_HPP
#define EARSHOT_CLI_ARGS_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace earshot::cli {

enum class Format {
    Txt,
    Srt,
    Vtt,
    Json,
};

enum class Device {
    Auto,
    Cpu,
    Cuda,
};

struct Options {
    std::string model = "large-v3-turbo";
    std::string language = "auto";
    bool translate = false;

    bool vad = true;
    float vad_threshold = 0.5F;
    std::int64_t from_ms = 0;
    std::int64_t duration_ms = 0;

    std::string output;
    std::vector<Format> formats{Format::Txt};
    bool paragraphs = false;
    bool to_stdout = false;
    bool force = false;

    bool quiet = false;
    bool verbose = false;

    Device device = Device::Auto;
    int gpu = 0;
    int threads = 0;

    std::string config_path;
    bool no_config = false;
    std::string models_dir;

    bool help = false;
    bool version = false;
    bool list_models = false;

    std::vector<std::string> inputs;
};

struct ParseResult {
    Options options;
    std::string error;

    [[nodiscard]] bool ok() const { return error.empty(); }
};

// Parses GNU style options. The vector overload accepts either a full argv
// (including "earshot" as the first element) or only the arguments.
[[nodiscard]] ParseResult parse_args(int argc, const char* const* argv);
[[nodiscard]] ParseResult parse_args(int argc, const char* const* argv, const Options& defaults);
[[nodiscard]] ParseResult parse_args(const std::vector<std::string>& args);
[[nodiscard]] ParseResult parse_args(const std::vector<std::string>& args, const Options& defaults);

[[nodiscard]] const char* format_name(Format format);
[[nodiscard]] const char* format_extension(Format format);
[[nodiscard]] bool parse_format_list(const std::string& list, std::vector<Format>& out,
                                     std::string& error);
[[nodiscard]] const char* device_name(Device device);
[[nodiscard]] bool parse_device(const std::string& text, Device& out);

[[nodiscard]] std::string usage_line();
[[nodiscard]] std::string help_text();

}  // namespace earshot::cli

#endif  // EARSHOT_CLI_ARGS_HPP
