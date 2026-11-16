#pragma once

#include "dwarf/dwarf_reader.h"

#include <cstdint>
#include <optional>
#include <ostream>
#include <vector>

namespace sys_sight {

// Variables larger than this are listed without their value.
inline constexpr std::uint64_t kMaxVariableBytes = 4096;

// A local variable and the bytes it held at the snapshot. `bytes` is empty if
// the variable was too large or could not be read.
struct FrameVariable {
    LocalVariable variable;
    std::optional<std::vector<std::uint8_t>> bytes;
};

// Writes the value of `type` found in `data`, which holds `type.size` bytes.
void write_value_json(std::ostream& output, const DwarfType& type, const std::uint8_t* data);

// Writes a JSON array of variables with their typed values.
void write_variables_json(std::ostream& output, const std::vector<FrameVariable>& variables);

}  // namespace sys_sight
