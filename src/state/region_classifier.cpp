#include "state/region_classifier.h"

namespace sys_sight {

std::string_view to_string(RegionKind kind) noexcept {
    switch (kind) {
        case RegionKind::Text: return "text";
        case RegionKind::Rodata: return "rodata";
        case RegionKind::Data: return "data";
        case RegionKind::Heap: return "heap";
        case RegionKind::Mmap: return "mmap";
        case RegionKind::Stack: return "stack";
        case RegionKind::Kernel: return "kernel";
    }
    return "mmap";
}

std::vector<ClassifiedRegion> classify_regions(const std::vector<ProcMapsEntry>& entries,
                                               const std::string& executable_path) {
    std::vector<ClassifiedRegion> regions;
    regions.reserve(entries.size());
    for (const auto& entry : entries) {
        const auto& permissions = entry.permissions;
        const bool writable = permissions.size() >= 2 && permissions[1] == 'w';
        const bool executable = permissions.size() >= 3 && permissions[2] == 'x';

        RegionKind kind = RegionKind::Mmap;
        if (entry.name == "[stack]") {
            kind = RegionKind::Stack;
        } else if (entry.name == "[heap]") {
            kind = RegionKind::Heap;
        } else if (entry.name.rfind("[v", 0) == 0) {  // [vdso], [vvar], [vsyscall]
            kind = RegionKind::Kernel;
        } else if (!executable_path.empty() && entry.name == executable_path) {
            kind = executable ? RegionKind::Text : writable ? RegionKind::Data : RegionKind::Rodata;
        } else if (entry.name.empty() && writable && !regions.empty() &&
                   regions.back().kind == RegionKind::Data &&
                   regions.back().entry.end == entry.start) {
            kind = RegionKind::Data;  // .bss pages beyond the file-backed segment
        }
        regions.push_back({entry, kind});
    }
    return regions;
}

}  // namespace sys_sight
