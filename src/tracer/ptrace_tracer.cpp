#include "tracer/ptrace_tracer.h"

#include "output/jsonl_writer.h"

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

PtraceTracer::PtraceTracer(std::string executable) : executable_(std::move(executable)) {}

int PtraceTracer::run(std::ostream& output) {
    const pid_t child = fork();
    if (child == -1) {
        return errno;
    }
    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
            _exit(127);
        }
        execl(executable_.c_str(), executable_.c_str(), static_cast<char*>(nullptr));
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
            static_cast<std::int64_t>(registers.rax),
            now_ns(),
        };
        write_jsonl(output, event);
        in_syscall = !in_syscall;
    }
}

}  // namespace sys_sight
