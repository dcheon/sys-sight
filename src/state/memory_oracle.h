#pragma once

#include "state/memory_map.h"
#include "state/proc_maps.h"

#include <cstdint>
#include <vector>

namespace sys_sight {

// An address range where the reducer and /proc disagree. A protection of -1
// means "not mapped".
struct MapMismatch {
    Address start;
    Address end;
    int reducer_protection;
    int proc_protection;
};

struct OracleResult {
    std::vector<MapMismatch> mismatches;
    // Bytes the kernel added below the stack without any system call; the
    // reducer cannot see this, so it is reported instead of counted as a mismatch.
    std::uint64_t stack_growth_bytes = 0;

    [[nodiscard]] bool matches() const noexcept { return mismatches.empty(); }
};

// Compares address coverage and protection (not names or offsets) between the
// state rebuilt from system calls and /proc/<pid>/maps.
[[nodiscard]] OracleResult compare_memory_maps(const std::vector<MemoryRegion>& reducer,
                                               const std::vector<ProcMapsEntry>& proc);

}  // namespace sys_sight
