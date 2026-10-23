#include "state/proc_bootstrap.h"

#include "state/proc_maps.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <climits>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace sys_sight {

BootstrapResult ProcBootstrap::seed(pid_t pid, ProcessState& state) {
    state.clear();
    seed_file_descriptors(pid, state);
    seed_memory_maps(pid, state);
    return {state.file_descriptors().size(), state.memory_map().regions().size()};
}

void ProcBootstrap::seed_file_descriptors(pid_t pid, ProcessState& state) {
    const std::string directory_path = "/proc/" + std::to_string(pid) + "/fd";
    DIR* directory = opendir(directory_path.c_str());
    if (directory == nullptr) {
        throw std::system_error(errno, std::generic_category(), "failed to open " + directory_path);
    }

    while (const dirent* entry = readdir(directory)) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        const auto descriptor = std::strtol(entry->d_name, nullptr, 10);
        if (descriptor < 0 || descriptor > INT32_MAX) {
            continue;
        }
        const std::string link_path = directory_path + "/" + entry->d_name;
        std::string target(4096, '\0');
        const auto length = readlink(link_path.c_str(), target.data(), target.size() - 1);
        if (length < 0) {
            continue;
        }
        target.resize(static_cast<std::size_t>(length));
        state.apply(OpenEvent{static_cast<FileDescriptor>(descriptor), std::move(target)});
    }
    closedir(directory);
}

void ProcBootstrap::seed_memory_maps(pid_t pid, ProcessState& state) {
    for (const auto& entry : read_proc_maps(pid)) {
        state.apply(MmapEvent{entry.start, entry.end - entry.start,
                              protection_from_permissions(entry.permissions), entry.name});
    }
}

}  // namespace sys_sight
