#include "cli/run_source.h"

#include "cli/embedded_marker.h"
#include "output/jsonl_writer.h"
#include "tracer/ptrace_tracer.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace sys_sight {
namespace {

namespace fs = std::filesystem;

// Build products live in a private temporary directory removed on scope exit.
class TempDirectory {
public:
    TempDirectory() {
        std::string pattern = (fs::temp_directory_path() / "sys-sight-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) {
            throw std::system_error(errno, std::generic_category(), "mkdtemp");
        }
        path_ = pattern;
    }
    ~TempDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

void write_failure(std::ostream& output, const char* code, const std::string& message) {
    output << "{\"error\":\"" << code << "\",\"message\":";
    write_json_string(output, message);
    output << "}\n";
}

// Runs the compiler directly (no shell). Its stdout goes to stderr so stdout
// stays pure JSONL. Returns the exit status, or -1 if it could not be started.
int run_compiler(const std::vector<std::string>& command) {
    const pid_t child = fork();
    if (child == -1) {
        return -1;
    }
    if (child == 0) {
        dup2(STDERR_FILENO, STDOUT_FILENO);
        std::vector<char*> arguments;
        for (const auto& argument : command) {
            arguments.push_back(const_cast<char*>(argument.c_str()));
        }
        arguments.push_back(nullptr);
        execvp(arguments.front(), arguments.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

}  // namespace

int run_source(const std::string& source, const std::vector<std::string>& arguments,
               std::ostream& output) {
    const fs::path source_path(source);
    if (source_path.extension() != ".c") {
        write_failure(output, "unsupported_source", "only C sources (.c) are supported: " + source);
        return 1;
    }
    std::error_code error;
    if (!fs::is_regular_file(source_path, error)) {
        write_failure(output, "source_unreadable", "cannot read source file: " + source);
        return 1;
    }

    std::vector<std::string> flags = {"-std=gnu11", "-O0", "-g", "-no-pie", "-fno-pie",
                                      "-fno-omit-frame-pointer"};
    try {
        const TempDirectory directory;
        const auto include_dir = directory.path() / "include";
        fs::create_directories(include_dir / "sys-sight");
        std::ofstream(include_dir / "sys-sight" / "marker.h") << kMarkerHeader;

        const auto binary = directory.path() / source_path.stem();
        const char* configured = std::getenv("CC");
        const std::string compiler = (configured != nullptr && *configured != '\0') ? configured : "cc";
        std::vector<std::string> command = {compiler};
        command.insert(command.end(), flags.begin(), flags.end());
        command.insert(command.end(), {"-I", include_dir.string(), source, "-o", binary.string()});

        output << std::flush;
        const auto started = std::chrono::steady_clock::now();
        const int status = run_compiler(command);
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - started).count();
        if (status != 0) {
            write_failure(output, "compile_failed",
                          status == 127 ? "compiler '" + compiler + "' could not be run"
                                        : "compiler '" + compiler + "' failed with status " +
                                              std::to_string(status));
            return 1;
        }

        output << "{\"build\":{\"compiler\":";
        write_json_string(output, compiler);
        output << ",\"flags\":[";
        for (std::size_t index = 0; index < flags.size(); ++index) {
            output << (index == 0 ? "" : ",");
            write_json_string(output, flags[index]);
        }
        output << "],\"duration_ms\":" << milliseconds << "}}\n";

        std::vector<std::string> traced = {binary.string()};
        traced.insert(traced.end(), arguments.begin(), arguments.end());
        return PtraceTracer(std::move(traced)).run(output);
    } catch (const std::exception& failure) {
        write_failure(output, "build_failed", failure.what());
        return 1;
    }
}

}  // namespace sys_sight
