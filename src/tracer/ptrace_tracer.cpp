#include "tracer/ptrace_tracer.h"

#include "output/jsonl_writer.h"
#include "state/process_state.h"
#include "syscall/syscall_decoder_x86_64.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
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

}  // namespace

PtraceTracer::PtraceTracer(std::vector<std::string> command) : command_(std::move(command)) {}

int PtraceTracer::run(std::ostream& output) {
    const pid_t child = fork();
    if (child == -1) {
        return errno;
    }
    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
            _exit(127);
        }
        std::vector<char*> arguments;
        arguments.reserve(command_.size() + 1);
        for (const auto& argument : command_) {
            arguments.push_back(const_cast<char*>(argument.c_str()));
        }
        arguments.push_back(nullptr);
        execvp(arguments.front(), arguments.data());
        _exit(127);
    }

    int status = 0;
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
    state.seed_standard_descriptors();
    while (true) {
        if (ptrace(PTRACE_SYSCALL, child, nullptr, nullptr) == -1) {
            return errno;
        }
        if (waitpid(child, &status, 0) == -1) {
            return errno;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            return 0;
        }
        if (!WIFSTOPPED(status) || WSTOPSIG(status) != (SIGTRAP | 0x80)) {
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
        in_syscall = !in_syscall;
    }
}

}  // namespace sys_sight
