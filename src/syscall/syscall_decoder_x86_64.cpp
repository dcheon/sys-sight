#include "syscall/syscall_decoder_x86_64.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>

namespace sys_sight {

std::string_view syscall_name_x86_64(std::int64_t number) noexcept {
    switch (number) {
        case SYS_read: return "read";
        case SYS_write: return "write";
        case SYS_close: return "close";
        case SYS_dup: return "dup";
        case SYS_dup2: return "dup2";
        case SYS_dup3: return "dup3";
        case SYS_openat: return "openat";
        case SYS_mmap: return "mmap";
        case SYS_munmap: return "munmap";
        case SYS_mprotect: return "mprotect";
        case SYS_mremap: return "mremap";
        case SYS_brk: return "brk";
        case SYS_execve: return "execve";
        case SYS_exit: return "exit";
        case SYS_exit_group: return "exit_group";
        default: return "unknown";
    }
}

std::optional<StateEvent> state_event_from_syscall(const SyscallEvent& event) {
    if (event.entry || event.return_value < 0) {
        return std::nullopt;
    }

    switch (event.number) {
        case SYS_openat:
            return OpenEvent{static_cast<FileDescriptor>(event.return_value), "<openat>"};
        case SYS_close:
            return CloseEvent{static_cast<FileDescriptor>(event.arguments[0])};
        case SYS_dup:
            return DupEvent{static_cast<FileDescriptor>(event.arguments[0]),
                            static_cast<FileDescriptor>(event.return_value)};
        case SYS_dup2:
        case SYS_dup3:
            return DupEvent{static_cast<FileDescriptor>(event.arguments[0]),
                            static_cast<FileDescriptor>(event.return_value)};
        case SYS_mmap:
            return MmapEvent{static_cast<Address>(event.return_value), event.arguments[1],
                             static_cast<int>(event.arguments[2]), "<mmap>"};
        case SYS_munmap:
            return MunmapEvent{event.arguments[0], event.arguments[1]};
        case SYS_brk:
            return BrkEvent{0, static_cast<Address>(event.return_value)};
        default:
            return std::nullopt;
    }
}

}  // namespace sys_sight
