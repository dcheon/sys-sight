#pragma once

#include "state/proc_maps.h"

#include <string>
#include <string_view>
#include <vector>

namespace sys_sight {

enum class RegionKind { Text, Rodata, Data, Heap, Mmap, Stack, Kernel };

[[nodiscard]] std::string_view to_string(RegionKind kind) noexcept;

struct ClassifiedRegion {
    ProcMapsEntry entry;
    RegionKind kind;
};

// Classifies mappings by kernel-visible facts only. `Data` covers the
// executable's whole writable segment (.data and .bss); splitting those needs
// the ELF section headers.
[[nodiscard]] std::vector<ClassifiedRegion> classify_regions(
    const std::vector<ProcMapsEntry>& entries, const std::string& executable_path);

}  // namespace sys_sight
