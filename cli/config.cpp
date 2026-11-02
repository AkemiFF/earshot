#include "config.hpp"

#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace earshot::cli {
namespace {

struct Value {
    enum class Kind {
        String,
        Integer,
        Boolean,
        Number,
        Invalid,
    };

    Kind kind = Kind::Invalid;
    std::string text;
    std::int64_t integer = 0;
    bool boolean = false;
    double number = 0.0;
};

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

std::string strip_comment(const std::string& line) {
    std::string out;
    out.reserve(line.size());
    char quote = '\0';
    bool escaped = false;
    for (char c : line) {
        if (quote != '\0') {
            out += c;
            if (quote == '"' && escaped) {
                escaped = false;
            } else if (quote == '"' && c == '\\') {
                escaped = true;
            } else if (c == quote) {
                quote = '\0';
            }
            continue;
        }
        if (c == '#') {
            break;
        }
        if (c == '"' || c == '\'') {
            quote = c;
        }
        out += c;
    }
    return out;
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

bool parse_quoted(const std::string& raw, std::string& out) {
    if (raw.size() < 2) {
        return false;
    }
    const char quote = raw.front();
    if ((quote != '"' && quote != '\'') || raw.back() != quote) {
        return false;
    }
    const std::string body = raw.substr(1, raw.size() - 2);
    out.clear();
    if (quote == '\'') {
        if (body.find('\'') != std::string::npos) {
            return false;
        }
        out = body;
        return true;
    }

    for (std::size_t i = 0; i < body.size(); ++i) {
        const char c = body[i];
        if (c == '"') {
            return false;
        }
        if (c != '\\') {
            out += c;
            continue;
        }
        if (i + 1 >= body.size()) {
            return false;
        }
        ++i;
        const char escaped = body[i];
        switch (escaped) {
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            case 'r':
                out += '\r';
                break;
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            default:
                out += escaped;
                break;
        }
    }
    return true;
}

Value parse_value(const std::string& raw) {
    Value value;
    if (raw.empty()) {
        return value;
    }
    if (raw.front() == '"' || raw.front() == '\'') {
        if (parse_quoted(raw, value.text)) {
            value.kind = Value::Kind::String;
        }
        return value;
    }
    if (raw == "true" || raw == "false") {
        value.kind = Value::Kind::Boolean;
        value.boolean = raw == "true";
        return value;
    }
    if (parse_int64(raw, value.integer)) {
        value.kind = Value::Kind::Integer;
        return value;
    }

    errno = 0;
    char* end = nullptr;
    const double number = std::strtod(raw.c_str(), &end);
    if (end != raw.c_str() && end != nullptr && *end == '\0' && errno != ERANGE &&
        std::isfinite(number)) {
        value.kind = Value::Kind::Number;
        value.number = number;
    }
    return value;
}

bool valid_key(const std::string& key) {
    if (key.empty()) {
        return false;
    }
    for (char c : key) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

void warn(ConfigLoadResult& result, const std::string& path, std::size_t line,
          const std::string& message) {
    result.warnings.push_back(path + ":" + std::to_string(line) + ": " + message);
}

void type_warning(ConfigLoadResult& result, const std::string& path, std::size_t line,
                  const std::string& key, const char* expected) {
    warn(result, path, line,
         "key '" + key + "' expects " + std::string(expected) + ", ignoring the value");
}

void assign_model(ConfigLoadResult& result, const std::string& path, std::size_t line,
                  const std::string& key, const Value& value) {
    Config& config = result.config;
    std::optional<std::string>* target = nullptr;
    if (key == "name") {
        target = &config.model_name;
    } else if (key == "path") {
        target = &config.model_path;
    } else if (key == "language") {
        target = &config.language;
    }
    if (target == nullptr) {
        warn(result, path, line, "unknown key '" + key + "' in section [model]");
        return;
    }
    if (value.kind != Value::Kind::String) {
        type_warning(result, path, line, key, "a quoted string");
        return;
    }
    if (value.text.empty()) {
        warn(result, path, line, "key '" + key + "' must not be empty, ignoring the value");
        return;
    }
    *target = value.text;
}

void assign_device(ConfigLoadResult& result, const std::string& path, std::size_t line,
                   const std::string& key, const Value& value) {
    Config& config = result.config;
    if (key == "backend") {
        if (value.kind != Value::Kind::String) {
            type_warning(result, path, line, key, "a quoted string");
            return;
        }
        Device device = Device::Auto;
        if (!parse_device(value.text, device)) {
            warn(result, path, line,
                 "invalid backend '" + value.text +
                     "' (expected auto, cpu or cuda), ignoring the value");
            return;
        }
        config.device = device;
        return;
    }
    if (key == "gpu" || key == "threads") {
        if (value.kind != Value::Kind::Integer || value.integer < 0 ||
            value.integer > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
            warn(result, path, line,
                 "key '" + key + "' expects a non-negative integer, ignoring the value");
            return;
        }
        if (key == "gpu") {
            config.gpu = static_cast<int>(value.integer);
        } else {
            config.threads = static_cast<int>(value.integer);
        }
        return;
    }
    warn(result, path, line, "unknown key '" + key + "' in section [device]");
}

void assign_output(ConfigLoadResult& result, const std::string& path, std::size_t line,
                   const std::string& key, const Value& value) {
    Config& config = result.config;
    if (key == "format") {
        if (value.kind != Value::Kind::String) {
            type_warning(result, path, line, key, "a quoted string");
            return;
        }
        std::vector<Format> formats;
        std::string error;
        if (!parse_format_list(value.text, formats, error)) {
            warn(result, path, line, "invalid format list: " + error + ", ignoring the value");
            return;
        }
        config.formats = formats;
        return;
    }
    if (key == "paragraphs" || key == "vad" || key == "force") {
        if (value.kind != Value::Kind::Boolean) {
            type_warning(result, path, line, key, "true or false");
            return;
        }
        if (key == "paragraphs") {
            config.paragraphs = value.boolean;
        } else if (key == "vad") {
            config.vad = value.boolean;
        } else {
            config.force = value.boolean;
        }
        return;
    }
    if (key == "vad_threshold") {
        double threshold = 0.0;
        if (value.kind == Value::Kind::Integer) {
            threshold = static_cast<double>(value.integer);
        } else if (value.kind == Value::Kind::Number) {
            threshold = value.number;
        } else {
            type_warning(result, path, line, key, "a number");
            return;
        }
        if (threshold < 0.0 || threshold > 1.0) {
            warn(result, path, line,
                 "key 'vad_threshold' must be between 0.0 and 1.0, ignoring the value");
            return;
        }
        config.vad_threshold = static_cast<float>(threshold);
        return;
    }
    warn(result, path, line, "unknown key '" + key + "' in section [output]");
}

void assign_value(ConfigLoadResult& result, const std::string& section, const std::string& path,
                  std::size_t line, const std::string& key, const Value& value) {
    if (section == "model") {
        assign_model(result, path, line, key, value);
    } else if (section == "device") {
        assign_device(result, path, line, key, value);
    } else if (section == "output") {
        assign_output(result, path, line, key, value);
    }
}

}  // namespace

ConfigLoadResult load_config(const std::string& path) {
    ConfigLoadResult result;
    if (path.empty()) {
        return result;
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            return result;
        }
        warn(result, path, 0, "cannot open config file, ignoring it");
        return result;
    }

    std::string section;
    bool known_section = false;
    std::string raw;
    std::size_t line_number = 0;
    while (std::getline(input, raw)) {
        ++line_number;
        if (!raw.empty() && raw.back() == '\r') {
            raw.pop_back();
        }
        const std::string line = trim(strip_comment(raw));
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[') {
            if (line.back() != ']' || line.size() < 3) {
                warn(result, path, line_number, "malformed section header");
                section.clear();
                known_section = false;
                continue;
            }
            section = trim(line.substr(1, line.size() - 2));
            known_section = section == "model" || section == "device" || section == "output";
            if (!known_section) {
                warn(result, path, line_number, "unknown section [" + section + "]");
            }
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            warn(result, path, line_number, "expected 'key = value'");
            continue;
        }
        const std::string key = trim(line.substr(0, equals));
        const std::string raw_value = trim(line.substr(equals + 1));
        if (!valid_key(key)) {
            warn(result, path, line_number, "malformed key");
            continue;
        }
        if (section.empty()) {
            warn(result, path, line_number,
                 "key '" + key + "' is outside of a section, ignoring it");
            continue;
        }
        if (!known_section) {
            continue;
        }

        const Value value = parse_value(raw_value);
        if (value.kind == Value::Kind::Invalid) {
            warn(result, path, line_number, "invalid value for key '" + key + "', ignoring it");
            continue;
        }
        assign_value(result, section, path, line_number, key, value);
    }
    if (input.bad()) {
        warn(result, path, line_number, "error while reading config file");
    }
    return result;
}

void apply_config(const Config& config, Options& options) {
    if (config.model_name.has_value()) {
        options.model = *config.model_name;
    }
    if (config.model_path.has_value()) {
        options.model = *config.model_path;
    }
    if (config.language.has_value()) {
        options.language = *config.language;
    }
    if (config.device.has_value()) {
        options.device = *config.device;
    }
    if (config.gpu.has_value()) {
        options.gpu = *config.gpu;
    }
    if (config.threads.has_value()) {
        options.threads = *config.threads;
    }
    if (config.formats.has_value()) {
        options.formats = *config.formats;
    }
    if (config.paragraphs.has_value()) {
        options.paragraphs = *config.paragraphs;
    }
    if (config.vad.has_value()) {
        options.vad = *config.vad;
    }
    if (config.vad_threshold.has_value()) {
        options.vad_threshold = *config.vad_threshold;
    }
    if (config.force.has_value()) {
        options.force = *config.force;
    }
}

std::string default_config_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && xdg[0] != '\0') {
        return (std::filesystem::path(xdg) / "earshot" / "config.toml").string();
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return (std::filesystem::path(home) / ".config" / "earshot" / "config.toml").string();
    }
    return {};
}

}  // namespace earshot::cli
