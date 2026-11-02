#ifndef EARSHOT_CLI_PROGRESS_HPP
#define EARSHOT_CLI_PROGRESS_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace earshot::cli {

// Renders a single updating progress line on stderr for terminals. When
// stderr is not a terminal, one line is printed on stderr as each file
// finishes. --quiet disables all progress output.
class Progress {
public:
    explicit Progress(int total_files, bool quiet = false);
    Progress(int total_files, bool quiet, bool interactive, bool colour);
    ~Progress();

    Progress(const Progress&) = delete;
    Progress& operator=(const Progress&) = delete;

    void begin(int index, const std::string& name, std::int64_t duration_ms = 0);
    void update(int percent);
    void end(bool success, bool cancelled = false);
    void flush();

    [[nodiscard]] bool interactive() const { return interactive_; }
    [[nodiscard]] bool quiet() const { return quiet_; }

    [[nodiscard]] static bool interactive_supported(bool quiet);

private:
    void clear_line();
    void render(int percent);
    [[nodiscard]] std::string status_line(bool success, bool cancelled) const;
    [[nodiscard]] double speed() const;

    int total_;
    bool quiet_;
    bool interactive_;
    bool colour_;

    int index_ = 0;
    std::string name_;
    std::int64_t duration_ms_ = 0;
    int percent_ = 0;
    std::chrono::steady_clock::time_point started_;
    std::size_t last_width_ = 0;
    bool active_ = false;
};

}  // namespace earshot::cli

#endif  // EARSHOT_CLI_PROGRESS_HPP
