#ifndef EARSHOT_CLI_CONFIG_HPP
#define EARSHOT_CLI_CONFIG_HPP

#include <optional>
#include <string>
#include <vector>

#include "args.hpp"

namespace earshot::cli {

// The optional values of the small TOML subset understood by earshot.
struct Config {
    std::optional<std::string> model_name;
    std::optional<std::string> model_path;
    std::optional<std::string> language;

    std::optional<Device> device;
    std::optional<int> gpu;
    std::optional<int> threads;

    std::optional<std::vector<Format>> formats;
    std::optional<bool> paragraphs;
    std::optional<bool> vad;
    std::optional<float> vad_threshold;
    std::optional<bool> force;
};

struct ConfigLoadResult {
    Config config;
    std::vector<std::string> warnings;
};

// Missing files are not an error. Malformed lines and unknown sections or
// keys are collected in `warnings`; they never make loading fail.
[[nodiscard]] ConfigLoadResult load_config(const std::string& path);

// Applies the values that are present. Command line options are applied
// afterwards and therefore take precedence.
void apply_config(const Config& config, Options& options);

// $XDG_CONFIG_HOME/earshot/config.toml, falling back to
// ~/.config/earshot/config.toml. Empty when neither variable is usable.
[[nodiscard]] std::string default_config_path();

}  // namespace earshot::cli

#endif  // EARSHOT_CLI_CONFIG_HPP
