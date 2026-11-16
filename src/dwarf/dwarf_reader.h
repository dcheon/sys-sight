#pragma once

#include "common/types.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct Dwarf;

namespace sys_sight {

enum class TypeKind {
    Signed,
    Unsigned,
    Char,
    Bool,
    Float,
    Pointer,
    Enum,
    Struct,
    Union,
    Array,
    Unknown,
};

struct DwarfType;
using DwarfTypePtr = std::shared_ptr<const DwarfType>;

struct DwarfMember {
    std::string name;
    std::uint64_t offset = 0;  // bytes from the start of the struct
    unsigned bit_size = 0;     // nonzero for bit-fields
    unsigned bit_offset = 0;   // bit-fields: bits from `offset`, little-endian
    DwarfTypePtr type;
};

struct DwarfEnumerator {
    std::string name;
    std::int64_t value = 0;
};

// A type with typedefs and qualifiers resolved. `name` is the C spelling the
// program used (for example "size_t" or "const char *"). Pointers keep only the
// name of what they point to, so the structure never loops.
struct DwarfType {
    TypeKind kind = TypeKind::Unknown;
    std::string name;
    std::uint64_t size = 0;
    bool is_signed = false;
    std::string target;                        // Pointer: the pointed-to type's name
    DwarfTypePtr element;                      // Array
    std::uint64_t count = 0;                   // Array
    std::vector<DwarfMember> members;          // Struct, Union
    std::vector<DwarfEnumerator> enumerators;  // Enum
};

struct LocalVariable {
    std::string name;
    bool parameter = false;
    Address address = 0;
    DwarfTypePtr type;
};

// Reads local variables from the DWARF debug information of an executable
// (libdw). Needs code built with `-g`.
class DwarfReader {
public:
    // Empty if the file has no readable DWARF.
    [[nodiscard]] static std::optional<DwarfReader> open(const std::string& path);

    DwarfReader(DwarfReader&& other) noexcept;
    DwarfReader& operator=(DwarfReader&&) = delete;
    DwarfReader(const DwarfReader&) = delete;
    DwarfReader& operator=(const DwarfReader&) = delete;
    ~DwarfReader();

    // The parameters and local variables in scope at `pc` (an address inside
    // the function, including nested blocks that contain it), with the
    // addresses they have in the frame whose frame pointer (rbp) is
    // `frame_pointer`. Requires code with frame pointers (-O0): variables
    // whose location this cannot compute are left out. For non-PIE
    // executables only.
    [[nodiscard]] std::vector<LocalVariable> locals_at(Address pc, Address frame_pointer) const;

private:
    DwarfReader(Dwarf* dwarf, int fd) : dwarf_(dwarf), fd_(fd) {}

    Dwarf* dwarf_;
    int fd_;
};

}  // namespace sys_sight
