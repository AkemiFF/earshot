#ifndef EARSHOT_CLI_MODELS_HPP
#define EARSHOT_CLI_MODELS_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace earshot::cli {

struct ModelInfo {
    std::string name;
    std::string file_name;
    std::uint64_t size_bytes = 0;
    std::string size_label;
    std::string url;
};

struct ResolveModelResult {
    std::string path;
    std::string display_name;
    std::uint64_t size_bytes = 0;
    bool downloaded = false;
    bool usage_error = false;
    std::string error;
    std::vector<std::string> warnings;

    [[nodiscard]] bool ok() const { return error.empty(); }
};

[[nodiscard]] const std::vector<ModelInfo>& model_catalogue();
[[nodiscard]] const std::vector<ModelInfo>& catalogue();
[[nodiscard]] const ModelInfo* find_model(const std::string& name);
[[nodiscard]] const ModelInfo& default_model();
[[nodiscard]] const ModelInfo& vad_model();

// $XDG_CACHE_HOME/earshot/models, falling back to ~/.cache/earshot/models.
[[nodiscard]] std::string default_models_dir();

// $EARSHOT_MODELS_DIR takes precedence over `override_dir`, then the XDG
// default. The directory is not created here.
[[nodiscard]] std::string resolve_models_dir(const std::string& override_dir = {});

[[nodiscard]] std::string model_file_path(const ModelInfo& model, const std::string& models_dir);
[[nodiscard]] std::string format_size(std::uint64_t bytes);

// Checks the "ggml" magic and, when `expected_bytes` is non-zero, that the
// file is at least 90% of the expected size.
[[nodiscard]] bool has_valid_model_file(const std::string& path, std::uint64_t expected_bytes);

// Resolves a catalogue name or a path. Catalogue entries are downloaded into
// the model cache when missing. Existing catalogue files are verified and
// re-downloaded when corrupt; an explicitly named path is never replaced.
[[nodiscard]] ResolveModelResult resolve_model(const std::string& name_or_path,
                                               const std::string& models_dir, bool quiet = false);
[[nodiscard]] ResolveModelResult resolve_vad_model(const std::string& models_dir,
                                                   bool quiet = false);

// Human readable catalogue, marking the default model with '*' and cached
// entries with the word "cached".
[[nodiscard]] std::string list_models(const std::string& models_dir);

}  // namespace earshot::cli

#endif  // EARSHOT_CLI_MODELS_HPP
