#pragma once

#include "common/types.h"

#include <sys/types.h>

#include <istream>
#include <string>
#include <string_view>
#include <vector>

namespace sys_sight {

// One line of /proc/<pid>/maps. `name` is the pathname column only
// (empty for anonymous mappings, "[stack]" and similar for special ones).
struct ProcMapsEntry {
    Address start;
    Address end;
    std::string permissions;
    std::string name;
};

// "rw-p" -> 3 (read = 1, write = 2, execute = 4); the private/shared flag is ignored.
[[nodiscard]] int protection_from_permissions(std::string_view permissions);

[[nodiscard]] std::vector<ProcMapsEntry> parse_proc_maps(std::istream& maps);
[[nodiscard]] std::vector<ProcMapsEntry> read_proc_maps(pid_t pid);

// Target of /proc/<pid>/exe, or empty if it cannot be read.
[[nodiscard]] std::string read_proc_exe(pid_t pid);

}  // namespace sys_sight
