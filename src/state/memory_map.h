#pragma once

#include "common/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace sys_sight {

struct MemoryRegion {
    Address start;
    Address end;
    int protection;
    std::string name;
};

class VirtualMemoryMap {
public:
    void map(Address start, std::uint64_t length, int protection, std::string name = {});
    void unmap(Address start, std::uint64_t length);
    void set_break(Address current_break);

    [[nodiscard]] const std::vector<MemoryRegion>& regions() const noexcept;
    [[nodiscard]] std::uint64_t mapped_bytes() const noexcept;
    [[nodiscard]] Address program_break() const noexcept;

private:
    void normalize();

    std::vector<MemoryRegion> regions_;
    Address program_break_ = 0;
};

}  // namespace sys_sight
