#pragma once

#include "common/types.h"

#include <sys/types.h>

#include <cstdint>
#include <optional>

namespace sys_sight {

// Reads a little-endian integer of 1, 2, 4 or 8 bytes from another process
// with a single process_vm_readv call. Empty if the size is unsupported or the
// address cannot be read.
[[nodiscard]] std::optional<std::uint64_t> read_tracee_integer(pid_t pid, Address address,
                                                               std::uint64_t size);

}  // namespace sys_sight
