#include "tracer/ptrace_tracer.h"

#include "elf/elf_reader.h"
#include "output/jsonl_writer.h"
#include "state/process_state.h"
#include "state/proc_bootstrap.h"
#include "state/proc_maps.h"
#include "state/region_classifier.h"
#include "state/stack_walker.h"
#include "syscall/syscall_decoder_x86_64.h"
#include "sys-sight/marker.h"
#include "tracer/tracee_memory.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include <utility>

namespace sys_sight {
namespace {

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void write_error(std::ostream& output, const char* code, const std::exception& error) {
    output << "{\"error\":\"" << code << "\",\"message\":";
    write_json_string(output, error.what());
    output << "}\n";
}

void write_snapshot(std::ostream& output, pid_t pid, std::int64_t sequence,
                    const user_regs_struct& registers) {
    const auto executable_path = read_proc_exe(pid);

    std::vector<ClassifiedRegion> regions;
    try {
        regions = classify_regions(read_proc_maps(pid), executable_path);
    } catch (const std::exception& error) {
        write_error(output, "maps_unreadable", error);
    }

    ElfInfo elf;
    try {
        elf = read_elf(executable_path);
        if (elf.pie) {
            // PIE addresses in the file are offsets, not runtime addresses.
            output << "{\"diagnostic\":{\"level\":\"warning\",\"code\":\"pie\","
                      "\"message\":\"PIE executable: sections and symbols omitted; "
                      "build with -no-pie\"}}\n";
            elf.sections.clear();
            elf.symbols.clear();
            elf.functions.clear();
        }
        for (auto& symbol : elf.symbols) {
            symbol.value = read_tracee_integer(pid, symbol.address, symbol.size);
        }
    } catch (const std::exception& error) {
        write_error(output, "elf_unreadable", error);
    }

    std::vector<AddressRange> text;
    for (const auto& region : regions) {
        if (region.kind == RegionKind::Text) {
            text.push_back({region.entry.start, region.entry.end});
        }
    }
    const auto walk = walk_stack(
        registers.rsp, registers.rbp,
        [pid](Address address) { return read_tracee_integer(pid, address, 8); }, elf.functions,
        text);
    if (walk.end == StackWalkEnd::NoFramePointer) {
        output << "{\"diagnostic\":{\"level\":\"warning\",\"code\":\"no_frame_pointer\","
                  "\"message\":";
        write_json_string(output, "function '" + walk.failed_function +
                                      "' does not set up a frame pointer (optimized build?); "
                                      "build with -O0 -fno-omit-frame-pointer");
        output << "}}\n";
    } else if (walk.frames.empty() || walk.end == StackWalkEnd::BrokenChain) {
        output << "{\"diagnostic\":{\"level\":\"warning\",\"code\":\"stack_walk\","
                  "\"message\":\"stack frames incomplete: build with -fno-omit-frame-pointer "
                  "and call the marker from the executable\"}}\n";
    }

    output << "{\"snapshot\":{\"trigger\":\"marker\",\"sequence\":" << sequence
           << ",\"registers\":{\"rip\":\"" << hex_address(registers.rip) << "\",\"rsp\":\""
           << hex_address(registers.rsp) << "\",\"rbp\":\"" << hex_address(registers.rbp)
           << "\"},\"regions\":";
    write_regions_json(output, regions);
    output << ",\"executable\":";
    write_executable_json(output, executable_path, elf);
    output << ",\"stack\":";
    write_stack_json(output, walk);
    output << "}}\n" << std::flush;
}

}  // namespace

PtraceTracer::PtraceTracer(std::vector<std::string> command) : command_(std::move(command)) {}

int PtraceTracer::run(std::ostream& output) {
    output << "{\"schema_version\":" << kSchemaVersion << "}\n";

    // The pipe closes on a successful exec; on failure the child sends errno.
    int exec_pipe[2];
    if (pipe2(exec_pipe, O_CLOEXEC) == -1) {
        return errno;
    }
    const pid_t child = fork();
    if (child == -1) {
        return errno;
    }
    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != -1) {
            std::vector<char*> arguments;
            arguments.reserve(command_.size() + 1);
            for (const auto& argument : command_) {
                arguments.push_back(const_cast<char*>(argument.c_str()));
            }
            arguments.push_back(nullptr);
            execvp(arguments.front(), arguments.data());
        }
        const int failure = errno;
        const ssize_t sent = write(exec_pipe[1], &failure, sizeof failure);
        (void)sent;
        _exit(127);
    }

    close(exec_pipe[1]);
    int exec_errno = 0;
    ssize_t received = 0;
    do {
        received = read(exec_pipe[0], &exec_errno, sizeof exec_errno);
    } while (received == -1 && errno == EINTR);
    close(exec_pipe[0]);

    int status = 0;
    if (received == static_cast<ssize_t>(sizeof exec_errno)) {
        waitpid(child, &status, 0);
        std::cerr << "sys-sight: cannot run '" << command_.front()
                  << "': " << std::strerror(exec_errno) << "\n";
        output << "{\"error\":\"exec_failed\",\"errno\":" << exec_errno << ",\"message\":\""
               << std::strerror(exec_errno) << "\"}\n";
        // Same codes as a shell: 127 for not found, 126 otherwise.
        return exec_errno == ENOENT ? 127 : 126;
    }
    if (waitpid(child, &status, 0) == -1) {
        return errno;
    }
    if (!WIFSTOPPED(status)) {
        return 1;
    }
    if (ptrace(PTRACE_SETOPTIONS, child, nullptr, PTRACE_O_TRACESYSGOOD) == -1) {
        return errno;
    }

    bool in_syscall = false;
    std::int64_t sequence = 0;
    ProcessState state;
    try {
        const auto bootstrap = ProcBootstrap::seed(child, state);
        output << "{\"bootstrap\":{\"file_descriptors\":" << bootstrap.file_descriptors
               << ",\"memory_regions\":" << bootstrap.memory_regions << "}}\n";
    } catch (const std::exception& error) {
        output << "{\"error\":\"bootstrap_failed\",\"message\":\"" << error.what() << "\"}\n";
        return 1;
    }
    int pending_signal = 0;
    while (true) {
        // Passing the signal on keeps a crashing tracee from looking healthy.
        if (ptrace(PTRACE_SYSCALL, child, nullptr,
                   reinterpret_cast<void*>(static_cast<std::intptr_t>(pending_signal))) == -1) {
            return errno;
        }
        pending_signal = 0;
        if (waitpid(child, &status, 0) == -1) {
            return errno;
        }
        if (WIFEXITED(status)) {
            output << "{\"exit\":{\"status\":" << WEXITSTATUS(status) << "}}\n";
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            output << "{\"exit\":{\"signal\":" << WTERMSIG(status) << "}}\n";
            return 128 + WTERMSIG(status);
        }
        if (!WIFSTOPPED(status) || WSTOPSIG(status) != (SIGTRAP | 0x80)) {
            if (WIFSTOPPED(status) && WSTOPSIG(status) != SIGTRAP) {
                pending_signal = WSTOPSIG(status);
            }
            continue;
        }

        user_regs_struct registers {};
        if (ptrace(PTRACE_GETREGS, child, nullptr, &registers) == -1) {
            return errno;
        }
        SyscallEvent event{
            sequence++,
            child,
            child,
            static_cast<std::int64_t>(registers.orig_rax),
            !in_syscall,
            {registers.rdi, registers.rsi, registers.rdx, registers.r10, registers.r8,
             registers.r9},
            static_cast<std::int64_t>(registers.rax),
            now_ns(),
            std::string(syscall_name_x86_64(static_cast<std::int64_t>(registers.orig_rax))),
        };
        if (const auto state_event = state_event_from_syscall(event)) {
            state.apply(*state_event);
        }
        write_jsonl(output, event);
        if (event.entry && event.number == SYSSIGHT_MARKER_SYSCALL) {
            // The tracee is already stopped at syscall entry; resuming it
            // returns -ENOSYS to the program.
            write_snapshot(output, child, event.sequence, registers);
        }
        in_syscall = !in_syscall;
    }
}

}  // namespace sys_sight
