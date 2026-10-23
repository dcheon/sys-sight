#pragma once

#include "common/types.h"
#include "elf/elf_reader.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace sys_sight {

struct AddressRange {
    Address start;
    Address end;
};

// One function's stack frame, [start, end): from the stack pointer it had
// while calling its callee up to and including its return address.
struct StackFrame {
    std::string function;  // empty if unknown (stripped or PIE)
    Address start;
    Address end;
    Address frame_pointer;
    Address saved_frame_pointer;
    Address return_address;
};

enum class StackWalkEnd {
    LeftExecutable,  // the next caller is not in the executable (for example libc)
    BrokenChain,     // the rbp chain is unreadable or inconsistent
    NoFramePointer,  // a function does not set up rbp (built with optimization?)
    DepthLimit,
};

[[nodiscard]] const char* to_string(StackWalkEnd end) noexcept;

struct StackWalk {
    std::vector<StackFrame> frames;  // innermost (the function that called the marker) first
    StackWalkEnd end = StackWalkEnd::LeftExecutable;
    std::string failed_function;     // set when end is NoFramePointer
};

using MemoryReader = std::function<std::optional<std::uint64_t>(Address)>;

// Walks the rbp chain at a marker stop. `rsp` is the stack pointer inside the
// syscall wrapper, where [rsp] holds the marker call's return address, so the
// calling function's own stack pointer is rsp + 8. `rbp` is still the caller's
// frame pointer. Requires code built with frame pointers: when a function's
// symbol is known, its first instructions are checked for the rbp setup, and
// the walk stops there instead of returning a plausible but wrong frame.
[[nodiscard]] StackWalk walk_stack(Address rsp, Address rbp, const MemoryReader& read,
                                   const std::vector<ElfFunction>& functions,
                                   const std::vector<AddressRange>& text,
                                   std::size_t max_depth = 64);

}  // namespace sys_sight
