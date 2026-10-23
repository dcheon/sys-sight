#pragma once

#include "common/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sys_sight {

// An allocated (loaded) section. Addresses are those in the file, which are
// runtime addresses only for non-PIE executables.
struct ElfSection {
    std::string name;
    Address start;
    Address end;
};

// A data object from the symbol table. `value` is filled from the running
// process at snapshot time, for objects of 1, 2, 4 or 8 bytes.
struct ElfSymbol {
    std::string name;
    Address address;
    std::uint64_t size;
    std::string section;
    std::optional<std::uint64_t> value;
};

struct ElfInfo {
    bool pie = false;
    std::vector<ElfSection> sections;
    std::vector<ElfSymbol> symbols;  // empty for stripped executables
};

// Reads a 64-bit little-endian x86-64 ELF file. Throws on unreadable or
// malformed files; every offset is bounds-checked.
[[nodiscard]] ElfInfo read_elf(const std::string& path);

}  // namespace sys_sight
