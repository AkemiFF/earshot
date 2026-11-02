#include "models.hpp"

#include <sys/stat.h>
#include <sys/wait.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace earshot::cli {
namespace {

const char* environment(const char* name) {
    return std::getenv(name);
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

bool shell_command_ok(int status) {
    if (status == -1) {
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::string shell_quote(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 2);
    out += '\'';
    for (char c : text) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += '\'';
    return out;
}

bool command_available(const char* name) {
    const std::string command = "command -v " + shell_quote(name) + " >/dev/null 2>&1";
    return shell_command_ok(std::system(command.c_str()));
}

bool ensure_directory(const std::string& directory, std::string& error) {
    if (directory.empty()) {
        error = "the model cache directory is not set";
        return false;
    }
    std::error_code code;
    if (std::filesystem::exists(directory, code)) {
        if (!std::filesystem::is_directory(directory, code)) {
            error = "model cache path '" + directory + "' is not a directory";
            return false;
        }
        return true;
    }
    if (!std::filesystem::create_directories(directory, code) || code) {
        error = "cannot create model cache directory '" + directory + "'";
        return false;
    }
    ::chmod(directory.c_str(), static_cast<mode_t>(0755));
    return true;
}

std::string downloaded_part_path(const std::string& destination) {
    return destination + ".part";
}

bool download_file(const std::string& url, const std::string& destination,
                   std::uint64_t expected_bytes, bool quiet, std::string& error) {
    const std::string part = downloaded_part_path(destination);
    const std::string suffix = quiet ? " >/dev/null 2>&1" : "";
    bool ok = false;
    bool attempted = false;

    if (command_available("curl")) {
        attempted = true;
        const std::string command = "curl -L --fail --progress-bar -C - -o " + shell_quote(part) +
                                    " " + shell_quote(url) + suffix;
        ok = shell_command_ok(std::system(command.c_str()));
    }
    if (!ok && command_available("wget")) {
        attempted = true;
        const std::string command =
            "wget -c -O " + shell_quote(part) + " " + shell_quote(url) + suffix;
        ok = shell_command_ok(std::system(command.c_str()));
    }
    if (!ok) {
        std::error_code ignored;
        std::filesystem::remove(part, ignored);
        if (!attempted) {
            error = "no download tool found: install curl or wget";
        } else {
            error = "could not download '" + url + "'";
        }
        return false;
    }

    if (!has_valid_model_file(part, expected_bytes)) {
        std::error_code ignored;
        std::filesystem::remove(part, ignored);
        error = "the downloaded model is incomplete or corrupt";
        return false;
    }

    std::error_code code;
    if (std::filesystem::exists(destination, code)) {
        if (has_valid_model_file(destination, expected_bytes)) {
            std::filesystem::remove(part, code);
            return true;
        }
        std::filesystem::remove(destination, code);
    }
    std::filesystem::rename(part, destination, code);
    if (code) {
        std::error_code ignored;
        std::filesystem::remove(part, ignored);
        error = "cannot move the downloaded model into place at '" + destination + "'";
        return false;
    }
    return true;
}

void add_warning(ResolveModelResult& result, const std::string& message) {
    result.warnings.push_back(message);
}

bool looks_like_path(const std::string& value) {
    if (value.find('/') != std::string::npos) {
        return true;
    }
    if (ends_with(lowercase_ascii(value), ".bin")) {
        return true;
    }
    std::error_code code;
    return std::filesystem::exists(value, code);
}

bool cached_model(const ModelInfo& model, const std::string& models_dir) {
    std::error_code code;
    return std::filesystem::is_regular_file(model_file_path(model, models_dir), code) && !code;
}

}  // namespace

const std::vector<ModelInfo>& model_catalogue() {
    static const std::vector<ModelInfo> entries = {
        {"tiny", "ggml-tiny.bin", 75000000ULL, "75 MB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin"},
        {"tiny.en", "ggml-tiny.en.bin", 75000000ULL, "75 MB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.en.bin"},
        {"base", "ggml-base.bin", 142000000ULL, "142 MB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"},
        {"base.en", "ggml-base.en.bin", 142000000ULL, "142 MB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin"},
        {"small", "ggml-small.bin", 466000000ULL, "466 MB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin"},
        {"small.en", "ggml-small.en.bin", 466000000ULL, "466 MB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin"},
        {"medium", "ggml-medium.bin", 1500000000ULL, "1.5 GB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin"},
        {"medium.en", "ggml-medium.en.bin", 1500000000ULL, "1.5 GB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.en.bin"},
        {"large-v3", "ggml-large-v3.bin", 3100000000ULL, "3.1 GB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin"},
        {"large-v3-turbo", "ggml-large-v3-turbo.bin", 1600000000ULL, "1.6 GB",
         "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo.bin"},
    };
    return entries;
}

const std::vector<ModelInfo>& catalogue() {
    return model_catalogue();
}

const ModelInfo* find_model(const std::string& name) {
    for (const ModelInfo& model : model_catalogue()) {
        if (model.name == name) {
            return &model;
        }
    }
    return nullptr;
}

const ModelInfo& default_model() {
    const ModelInfo* model = find_model("large-v3-turbo");
    if (model != nullptr) {
        return *model;
    }
    return model_catalogue().back();
}

const ModelInfo& vad_model() {
    static const ModelInfo model = {
        "silero-v5.1.2",
        "ggml-silero-v5.1.2.bin",
        860000ULL,
        "860 KB",
        "https://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v5.1.2.bin",
    };
    return model;
}

std::string default_models_dir() {
    const char* xdg = environment("XDG_CACHE_HOME");
    if (xdg != nullptr && xdg[0] != '\0') {
        return (std::filesystem::path(xdg) / "earshot" / "models").string();
    }
    const char* home = environment("HOME");
    if (home != nullptr && home[0] != '\0') {
        return (std::filesystem::path(home) / ".cache" / "earshot" / "models").string();
    }
    std::error_code code;
    const std::filesystem::path current = std::filesystem::current_path(code);
    if (code) {
        return ".cache/earshot/models";
    }
    return (current / ".cache" / "earshot" / "models").string();
}

std::string resolve_models_dir(const std::string& override_dir) {
    const char* configured = environment("EARSHOT_MODELS_DIR");
    if (configured != nullptr && configured[0] != '\0') {
        return configured;
    }
    if (!override_dir.empty()) {
        return override_dir;
    }
    return default_models_dir();
}

std::string model_file_path(const ModelInfo& model, const std::string& models_dir) {
    if (models_dir.empty()) {
        return model.file_name;
    }
    return (std::filesystem::path(models_dir) / model.file_name).string();
}

std::string format_size(std::uint64_t bytes) {
    std::ostringstream out;
    constexpr double gigabyte = 1000000000.0;
    constexpr double megabyte = 1000000.0;
    constexpr double kilobyte = 1000.0;
    if (static_cast<double>(bytes) >= gigabyte) {
        out << std::fixed << std::setprecision(1) << static_cast<double>(bytes) / gigabyte << " GB";
    } else if (static_cast<double>(bytes) >= megabyte) {
        out << bytes / 1000000ULL << " MB";
    } else if (static_cast<double>(bytes) >= kilobyte) {
        out << bytes / 1000ULL << " KB";
    } else {
        out << bytes << " B";
    }
    return out.str();
}

bool has_valid_model_file(const std::string& path, std::uint64_t expected_bytes) {
    std::error_code code;
    if (!std::filesystem::is_regular_file(path, code)) {
        return false;
    }
    const std::uint64_t size = std::filesystem::file_size(path, code);
    if (code || size < 4) {
        return false;
    }
    if (expected_bytes > 0 && size < expected_bytes - expected_bytes / 10) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    char magic[4] = {'\0', '\0', '\0', '\0'};
    input.read(magic, 4);
    if (input.gcount() != 4) {
        return false;
    }
    // The GGML magic is the little-endian integer 0x67676d6c, therefore the
    // bytes on disk are "lmgg" on all normal Linux installations.
    return std::memcmp(magic, "lmgg", 4) == 0 || std::memcmp(magic, "ggml", 4) == 0;
}

ResolveModelResult resolve_model(const std::string& name_or_path, const std::string& models_dir,
                                 bool quiet) {
    ResolveModelResult result;
    if (name_or_path.empty()) {
        result.error = "model name is empty";
        result.usage_error = true;
        return result;
    }

    const ModelInfo* model = find_model(name_or_path);
    if (model != nullptr) {
        result.display_name = model->name;
        result.size_bytes = model->size_bytes;
        result.path = model_file_path(*model, models_dir);
        if (has_valid_model_file(result.path, model->size_bytes)) {
            return result;
        }
        std::error_code cache_error;
        if (std::filesystem::exists(result.path, cache_error)) {
            add_warning(result,
                        "cached model '" + result.path + "' is corrupt, downloading it again");
            std::error_code ignored;
            std::filesystem::remove(result.path, ignored);
        }

        std::string error;
        if (!ensure_directory(models_dir, error)) {
            result.error = error;
            return result;
        }
        if (!download_file(model->url, result.path, model->size_bytes, quiet, error)) {
            result.error = error;
            return result;
        }
        std::error_code size_error;
        const std::uint64_t actual_size = std::filesystem::file_size(result.path, size_error);
        if (!size_error) {
            result.size_bytes = actual_size;
        }
        result.downloaded = true;
        return result;
    }

    if (!looks_like_path(name_or_path)) {
        result.error = "unknown model '" + name_or_path +
                       "'\ntry 'earshot --list-models' to see the catalogue";
        result.usage_error = true;
        return result;
    }

    result.path = name_or_path;
    result.display_name = std::filesystem::path(name_or_path).filename().string();
    if (result.display_name.empty()) {
        result.display_name = name_or_path;
    }
    std::error_code code;
    if (!std::filesystem::exists(name_or_path, code)) {
        result.error = "model file '" + name_or_path + "' does not exist";
        return result;
    }
    if (!std::filesystem::is_regular_file(name_or_path, code)) {
        result.error = "model path '" + name_or_path + "' is not a regular file";
        return result;
    }
    if (!has_valid_model_file(name_or_path, 0)) {
        result.error = "model file '" + name_or_path + "' is not a valid ggml whisper model";
        return result;
    }
    return result;
}

ResolveModelResult resolve_vad_model(const std::string& models_dir, bool quiet) {
    ResolveModelResult result;
    const ModelInfo& model = vad_model();
    result.display_name = model.name;
    result.size_bytes = model.size_bytes;
    result.path = model_file_path(model, models_dir);
    if (has_valid_model_file(result.path, model.size_bytes)) {
        return result;
    }
    std::error_code cache_error;
    if (std::filesystem::exists(result.path, cache_error)) {
        add_warning(result,
                    "cached VAD model '" + result.path + "' is corrupt, downloading it again");
        std::error_code remove_error;
        std::filesystem::remove(result.path, remove_error);
    }

    std::string error;
    if (!ensure_directory(models_dir, error)) {
        result.error = error;
        return result;
    }
    if (!download_file(model.url, result.path, model.size_bytes, quiet, error)) {
        result.error = error;
        return result;
    }
    std::error_code size_error;
    const std::uint64_t actual_size = std::filesystem::file_size(result.path, size_error);
    if (!size_error) {
        result.size_bytes = actual_size;
    }
    result.downloaded = true;
    return result;
}

std::string list_models(const std::string& models_dir) {
    std::size_t width = 0;
    for (const ModelInfo& model : model_catalogue()) {
        width = std::max(width, model.name.size());
    }

    std::ostringstream out;
    out << "Models in " << (models_dir.empty() ? "(no cache directory)" : models_dir) << ":\n";
    for (const ModelInfo& model : model_catalogue()) {
        const bool is_default = model.name == default_model().name;
        out << (is_default ? "* " : "  ") << std::left << std::setw(static_cast<int>(width))
            << model.name << "  " << std::right << std::setw(7) << model.size_label;
        if (cached_model(model, models_dir)) {
            const bool valid =
                has_valid_model_file(model_file_path(model, models_dir), model.size_bytes);
            out << (valid ? "  cached" : "  cached (invalid)");
        }
        out << '\n';
    }
    return out.str();
}

}  // namespace earshot::cli
