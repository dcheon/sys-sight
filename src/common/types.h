#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <variant>

namespace sys_sight {

using Address = std::uint64_t;
using FileDescriptor = int;
using SyscallArguments = std::array<std::uint64_t, 6>;

struct OpenEvent {
    FileDescriptor fd;
    std::string path;
};

struct CloseEvent {
    FileDescriptor fd;
};

struct DupEvent {
    FileDescriptor source;
    FileDescriptor target;
};

struct MmapEvent {
    Address start;
    std::uint64_t length;
    int protection;
    std::string name;
};

struct MunmapEvent {
    Address start;
    std::uint64_t length;
};

struct BrkEvent {
    Address previous_break;
    Address current_break;
};

using StateEvent = std::variant<OpenEvent, CloseEvent, DupEvent, MmapEvent, MunmapEvent, BrkEvent>;

}  // namespace sys_sight
