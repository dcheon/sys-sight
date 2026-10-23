#include "tracer/tracee_memory.h"

#include <sys/uio.h>

#include <cstring>

namespace sys_sight {

std::optional<std::uint64_t> read_tracee_integer(pid_t pid, Address address, std::uint64_t size) {
    if (size != 1 && size != 2 && size != 4 && size != 8) {
        return std::nullopt;
    }
    unsigned char buffer[8] = {};
    iovec local{buffer, static_cast<std::size_t>(size)};
    iovec remote{reinterpret_cast<void*>(address), static_cast<std::size_t>(size)};
    if (process_vm_readv(pid, &local, 1, &remote, 1, 0) != static_cast<ssize_t>(size)) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    std::memcpy(&value, buffer, sizeof value);  // x86-64 is little-endian
    return value;
}

}  // namespace sys_sight
