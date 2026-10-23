#pragma once

#include "common/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sys_sight {

inline constexpr std::uint64_t kPageSize = 4096;  // Linux x86-64

struct MemoryRegion {
    Address start;
    Address end;
    int protection;
    std::string name;
};

// Virtual address space as the kernel keeps it. Lengths passed in are rounded
// up to whole pages, as the kernel does.
class VirtualMemoryMap {
public:
    void clear() noexcept;
    // Replaces whatever the new mapping overlaps (MAP_FIXED semantics).
    void map(Address start, std::uint64_t length, int protection, std::string name = {});
    void unmap(Address start, std::uint64_t length);
    // Changes the protection of the mapped parts of the range.
    void protect(Address start, std::uint64_t length, int protection);
    // Applies a successful mremap; `new_start` is its return value.
    void remap(Address old_start, std::uint64_t old_length, Address new_start,
               std::uint64_t new_length, bool keep_old);
    // Applies a successful brk. `requested` 0 is a query and teaches the heap's
    // start; later calls grow or shrink the heap mapping. If the first call is
    // not a query the heap start is unknown and the heap is not tracked.
    void set_break(Address requested, Address current_break);

    [[nodiscard]] const std::vector<MemoryRegion>& regions() const noexcept;
    [[nodiscard]] std::uint64_t mapped_bytes() const noexcept;
    [[nodiscard]] Address program_break() const noexcept;

private:
    void remove(Address start, Address end);
    void normalize();

    std::vector<MemoryRegion> regions_;
    Address program_break_ = 0;
    bool break_known_ = false;
    Address heap_start_ = 0;  // page aligned
    Address heap_end_ = 0;    // page aligned end of the heap mapping
};

}  // namespace sys_sight
