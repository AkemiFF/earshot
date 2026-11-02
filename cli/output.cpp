#include "output.hpp"

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include "earshot/formats.hpp"

namespace earshot::cli {
namespace {

bool ends_with_slash(const std::string& text) {
    return !text.empty() && (text.back() == '/' || text.back() == '\\');
}

bool is_directory(const std::string& path) {
    std::error_code code;
    return std::filesystem::is_directory(path, code) && !code;
}

bool has_format_extension(const std::string& path) {
    const std::string extension = std::filesystem::path(path).extension().string();
    if (extension.size() < 2) {
        return false;
    }
    return formats::is_supported_format(extension.substr(1));
}

std::string join_path(const std::string& directory, const std::string& name) {
    if (directory.empty()) {
        return name;
    }
    return (std::filesystem::path(directory) / name).string();
}

std::string with_extension(const std::string& stem, Format format) {
    return stem + "." + format_extension(format);
}

std::string temporary_name(const std::filesystem::path& target) {
    std::string name = target.filename().string();
    if (name.empty()) {
        name = "earshot";
    }
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return "." + name + ".tmp." + std::to_string(static_cast<long long>(::getpid())) + "." +
           std::to_string(static_cast<long long>(stamp));
}

}  // namespace

std::string input_stem(const std::string& path) {
    const std::filesystem::path file(path);
    std::string stem = file.stem().string();
    if (stem.empty() || stem == "." || stem == "..") {
        stem = file.filename().string();
    }
    if (stem.empty()) {
        stem = "transcript";
    }
    return stem;
}

OutputPlan plan_outputs(const Options& options, const std::string& input, std::size_t input_count) {
    OutputPlan plan;
    if (options.formats.empty()) {
        plan.error = "no output formats selected";
        return plan;
    }
    if (options.to_stdout || options.output == "-") {
        for (Format format : options.formats) {
            OutputTarget target;
            target.format = format;
            target.path = "-";
            target.to_stdout = true;
            plan.targets.push_back(target);
        }
        return plan;
    }

    if (options.output.empty()) {
        const std::filesystem::path input_path(input);
        const std::string directory = input_path.parent_path().string();
        const std::string stem = input_stem(input);
        for (Format format : options.formats) {
            OutputTarget target;
            target.format = format;
            target.path = join_path(directory, with_extension(stem, format));
            plan.targets.push_back(target);
        }
        return plan;
    }

    bool directory =
        input_count > 1 || ends_with_slash(options.output) || is_directory(options.output);
    if (input_count > 1) {
        std::error_code code;
        const bool exists = std::filesystem::exists(options.output, code);
        if (!code && exists && !std::filesystem::is_directory(options.output, code)) {
            plan.error = "output path '" + options.output +
                         "' is not a directory (several inputs were given)";
            return plan;
        }
    }

    if (directory) {
        const std::string stem = input_stem(input);
        for (Format format : options.formats) {
            OutputTarget target;
            target.format = format;
            target.path = join_path(options.output, with_extension(stem, format));
            plan.targets.push_back(target);
        }
        return plan;
    }

    if (options.formats.size() == 1) {
        OutputTarget target;
        target.format = options.formats.front();
        target.path = options.output;
        plan.targets.push_back(target);
        return plan;
    }

    std::string base = options.output;
    if (has_format_extension(base)) {
        base = std::filesystem::path(base).replace_extension().string();
    }
    for (Format format : options.formats) {
        OutputTarget target;
        target.format = format;
        target.path = with_extension(base, format);
        plan.targets.push_back(target);
    }
    return plan;
}

std::string render(const earshot::Result& result, Format format, bool paragraphs,
                   const std::string& source_path) {
    switch (format) {
        case Format::Txt:
            return formats::to_txt(
                result, paragraphs ? formats::TxtStyle::Paragraphs : formats::TxtStyle::Lines);
        case Format::Srt:
            return formats::to_srt(result);
        case Format::Vtt:
            return formats::to_vtt(result);
        case Format::Json:
            return formats::to_json(result, source_path);
    }
    return {};
}

earshot::Result clip_result(const earshot::Result& result, std::int64_t from_ms,
                            std::int64_t duration_ms) {
    if (from_ms <= 0 && duration_ms <= 0) {
        return result;
    }

    const std::int64_t start = std::max<std::int64_t>(from_ms, 0);
    const std::int64_t end =
        duration_ms > 0 && start <= std::numeric_limits<std::int64_t>::max() - duration_ms
            ? start + duration_ms
            : std::numeric_limits<std::int64_t>::max();

    earshot::Result clipped;
    clipped.language = result.language;
    if (result.audio_duration_ms > 0) {
        const std::int64_t available = std::max<std::int64_t>(result.audio_duration_ms - start, 0);
        clipped.audio_duration_ms = duration_ms > 0 ? std::min(duration_ms, available) : available;
    }

    for (const earshot::Segment& segment : result.segments) {
        if (segment.t1_ms <= start || segment.t0_ms >= end) {
            continue;
        }
        earshot::Segment copy;
        copy.t0_ms = std::max(segment.t0_ms, start) - start;
        copy.t1_ms = std::min(segment.t1_ms, end) - start;
        if (copy.t1_ms < copy.t0_ms) {
            copy.t1_ms = copy.t0_ms;
        }
        copy.text = segment.text;
        copy.no_speech_prob = segment.no_speech_prob;
        clipped.segments.push_back(copy);
    }
    return clipped;
}

bool write_output(const OutputTarget& target, const std::string& content, bool force,
                  std::string& error) {
    if (target.to_stdout || target.path == "-") {
        std::cout.write(content.data(), static_cast<std::streamsize>(content.size()));
        std::cout.flush();
        if (!std::cout.good()) {
            error = "failed to write to stdout";
            return false;
        }
        return true;
    }

    const std::filesystem::path destination(target.path);
    if (destination.empty()) {
        error = "output path is empty";
        return false;
    }

    std::error_code code;
    const std::filesystem::path parent =
        destination.parent_path().empty() ? std::filesystem::path(".") : destination.parent_path();
    if (std::filesystem::exists(destination, code)) {
        if (code) {
            error = "cannot inspect output path '" + target.path + "'";
            return false;
        }
        if (std::filesystem::is_directory(destination, code)) {
            error = "output path '" + target.path + "' is a directory";
            return false;
        }
        if (!force) {
            error = "output file '" + target.path + "' already exists (use --force)";
            return false;
        }
    }

    std::filesystem::create_directories(parent, code);
    if (code) {
        error = "cannot create output directory '" + parent.string() + "'";
        return false;
    }

    const std::filesystem::path temporary = parent / temporary_name(destination);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            error = "cannot create temporary output file in '" + parent.string() + "'";
            return false;
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.close();
        if (!output) {
            std::filesystem::remove(temporary, code);
            error = "failed to write output file '" + target.path + "'";
            return false;
        }
    }

    if (!force) {
        code.clear();
        if (std::filesystem::exists(destination, code) && !code) {
            std::filesystem::remove(temporary, code);
            error = "output file '" + target.path + "' already exists (use --force)";
            return false;
        }
    }

    code.clear();
    std::filesystem::rename(temporary, destination, code);
    if (code) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = "cannot write output file '" + target.path + "'";
        return false;
    }
    return true;
}

}  // namespace earshot::cli
