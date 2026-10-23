#include "state/memory_map.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace sys_sight {
namespace {

constexpr int kHeapProtection = 3;  // read | write

std::uint64_t round_up(std::uint64_t length) {
    if (length > UINT64_MAX - (kPageSize - 1)) {
        throw std::invalid_argument("memory range length overflows");
    }
    return (length + kPageSize - 1) & ~(kPageSize - 1);
}

}  // namespace

void VirtualMemoryMap::clear() noexcept {
    regions_.clear();
    program_break_ = 0;
    break_known_ = false;
    heap_start_ = 0;
    heap_end_ = 0;
}

void VirtualMemoryMap::map(Address start, std::uint64_t length, int protection, std::string name) {
    if (length == 0) {
        throw std::invalid_argument("invalid memory mapping range");
    }
    const auto size = round_up(length);
    if (start > UINT64_MAX - size) {
        throw std::invalid_argument("invalid memory mapping range");
    }
    remove(start, start + size);
    regions_.push_back({start, start + size, protection, std::move(name)});
    normalize();
}

void VirtualMemoryMap::unmap(Address start, std::uint64_t length) {
    if (length == 0) {
        throw std::invalid_argument("invalid memory unmapping range");
    }
    const auto size = round_up(length);
    if (start > UINT64_MAX - size) {
        throw std::invalid_argument("invalid memory unmapping range");
    }
    remove(start, start + size);
}

void VirtualMemoryMap::protect(Address start, std::uint64_t length, int protection) {
    if (length == 0) {
        return;
    }
    const auto size = round_up(length);
    if (start > UINT64_MAX - size) {
        throw std::invalid_argument("invalid memory protection range");
    }
    const Address end = start + size;
    std::vector<MemoryRegion> result;
    for (const auto& region : regions_) {
        if (end <= region.start || start >= region.end) {
            result.push_back(region);
            continue;
        }
        if (region.start < start) {
            result.push_back({region.start, start, region.protection, region.name});
        }
        result.push_back({std::max(region.start, start), std::min(region.end, end), protection,
                          region.name});
        if (end < region.end) {
            result.push_back({end, region.end, region.protection, region.name});
        }
    }
    regions_ = std::move(result);
    normalize();
}

void VirtualMemoryMap::remap(Address old_start, std::uint64_t old_length, Address new_start,
                             std::uint64_t new_length, bool keep_old) {
    const auto source = std::find_if(regions_.begin(), regions_.end(), [old_start](const auto& region) {
        return old_start >= region.start && old_start < region.end;
    });
    if (source == regions_.end()) {
        return;
    }
    const int protection = source->protection;
    std::string name = source->name;
    if (!keep_old && old_length != 0) {
        unmap(old_start, old_length);
    }
    map(new_start, new_length, protection, std::move(name));
}

void VirtualMemoryMap::set_break(Address requested, Address current_break) {
    program_break_ = current_break;
    if (!break_known_) {
        if (requested != 0) {
            return;
        }
        break_known_ = true;
        heap_start_ = heap_end_ = round_up(current_break);
        return;
    }

    const Address new_end = std::max(round_up(current_break), heap_start_);
    if (new_end > heap_end_) {
        map(heap_end_, new_end - heap_end_, kHeapProtection, "[heap]");
    } else if (new_end < heap_end_) {
        remove(new_end, heap_end_);
    }
    heap_end_ = new_end;
}

const std::vector<MemoryRegion>& VirtualMemoryMap::regions() const noexcept {
    return regions_;
}

std::uint64_t VirtualMemoryMap::mapped_bytes() const noexcept {
    std::uint64_t total = 0;
    for (const auto& region : regions_) {
        total += region.end - region.start;
    }
    return total;
}

Address VirtualMemoryMap::program_break() const noexcept {
    return program_break_;
}

void VirtualMemoryMap::remove(Address start, Address end) {
    std::vector<MemoryRegion> result;
    for (const auto& region : regions_) {
        if (end <= region.start || start >= region.end) {
            result.push_back(region);
            continue;
        }
        if (region.start < start) {
            result.push_back({region.start, start, region.protection, region.name});
        }
        if (end < region.end) {
            result.push_back({end, region.end, region.protection, region.name});
        }
    }
    regions_ = std::move(result);
}

void VirtualMemoryMap::normalize() {
    std::sort(regions_.begin(), regions_.end(), [](const auto& left, const auto& right) {
        return left.start < right.start;
    });
    std::vector<MemoryRegion> result;
    for (const auto& region : regions_) {
        if (!result.empty()) {
            auto& previous = result.back();
            if (previous.end == region.start && previous.protection == region.protection &&
                previous.name == region.name) {
                previous.end = region.end;
                continue;
            }
        }
        result.push_back(region);
    }
    regions_ = std::move(result);
}

}  // namespace sys_sight
