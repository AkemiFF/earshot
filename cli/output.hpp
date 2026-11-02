#ifndef EARSHOT_CLI_OUTPUT_HPP
#define EARSHOT_CLI_OUTPUT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "args.hpp"
#include "earshot/earshot.hpp"

namespace earshot::cli {

struct OutputTarget {
    Format format = Format::Txt;
    std::string path;
    bool to_stdout = false;
};

struct OutputPlan {
    std::vector<OutputTarget> targets;
    std::string error;

    [[nodiscard]] bool ok() const { return error.empty(); }
};

// Builds one destination per requested format. The rules are:
//   --stdout / -o -      stdout for every format;
//   -o directory or '/'  directory, using the input stem plus the extension;
//   several inputs       -o is always treated as a directory;
//   -o file path         exact name for one format, otherwise a base name;
//   no -o                next to the input file.
[[nodiscard]] OutputPlan plan_outputs(const Options& options, const std::string& input,
                                      std::size_t input_count);

[[nodiscard]] std::string input_stem(const std::string& path);

// Serialises `result` in the requested format.
[[nodiscard]] std::string render(const earshot::Result& result, Format format, bool paragraphs,
                                 const std::string& source_path);

// Applies --from/--duration to an existing result, shifting timestamps so the
// selected window starts at zero.
[[nodiscard]] earshot::Result clip_result(const earshot::Result& result, std::int64_t from_ms,
                                          std::int64_t duration_ms);

// Writes atomically: a temporary file in the destination directory, then a
// rename. Parent directories are created. Returns false and fills `error`
// when an existing file would be overwritten without `force`.
[[nodiscard]] bool write_output(const OutputTarget& target, const std::string& content, bool force,
                                std::string& error);

}  // namespace earshot::cli

#endif  // EARSHOT_CLI_OUTPUT_HPP
