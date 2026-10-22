#include "state/memory_map.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace sys_sight {

void VirtualMemoryMap::clear() noexcept {
    regions_.clear();
    program_break_ = 0;
}

void VirtualMemoryMap::map(Address start, std::uint64_t length, int protection, std::string name) {
    if (length == 0 || start > UINT64_MAX - length) {
        throw std::invalid_argument("invalid memory mapping range");
    }
    regions_.push_back({start, start + length, protection, std::move(name)});
    normalize();
}

void VirtualMemoryMap::unmap(Address start, std::uint64_t length) {
    if (length == 0 || start > UINT64_MAX - length) {
        throw std::invalid_argument("invalid memory unmapping range");
    }
    const Address end = start + length;
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

void VirtualMemoryMap::set_break(Address current_break) {
    program_break_ = current_break;
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
