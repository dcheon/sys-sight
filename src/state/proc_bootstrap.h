#pragma once

#include "state/process_state.h"

#include <sys/types.h>

#include <string>

namespace sys_sight {

struct BootstrapResult {
    std::size_t file_descriptors = 0;
    std::size_t memory_regions = 0;
};

class ProcBootstrap {
public:
    [[nodiscard]] static BootstrapResult seed(pid_t pid, ProcessState& state);

private:
    static void seed_file_descriptors(pid_t pid, ProcessState& state);
    static void seed_memory_maps(pid_t pid, ProcessState& state);
};

}  // namespace sys_sight
