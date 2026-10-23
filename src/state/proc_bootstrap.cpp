#include "state/proc_bootstrap.h"

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
namespace {

std::uint64_t parse_hex(std::string_view value) {
    std::uint64_t result = 0;
    const auto* begin = value.data();
    const auto* end = begin + value.size();
    const auto parsed = std::from_chars(begin, end, result, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        throw std::runtime_error("invalid hexadecimal value in /proc maps");
    }
    return result;
}

std::string trim_left(std::string value) {
    const auto first = value.find_first_not_of(' ');
    return first == std::string::npos ? std::string{} : value.substr(first);
}

int protection_from_permissions(std::string_view permissions) {
    int protection = 0;
    if (permissions.size() >= 1 && permissions[0] == 'r') {
        protection |= 1;
    }
    if (permissions.size() >= 2 && permissions[1] == 'w') {
        protection |= 2;
    }
    if (permissions.size() >= 3 && permissions[2] == 'x') {
        protection |= 4;
    }
    return protection;
}

}  // namespace

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
    const std::string maps_path = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream maps(maps_path);
    if (!maps) {
        throw std::system_error(errno, std::generic_category(), "failed to open " + maps_path);
    }

    std::string line;
    while (std::getline(maps, line)) {
        const auto separator = line.find('-');
        const auto permissions_start = line.find(' ', separator);
        if (separator == std::string::npos || permissions_start == std::string::npos) {
            throw std::runtime_error("invalid line in /proc maps");
        }
        const auto permissions_end = line.find(' ', permissions_start + 1);
        if (permissions_end == std::string::npos) {
            throw std::runtime_error("invalid permissions in /proc maps");
        }
        const auto start = parse_hex(std::string_view(line).substr(0, separator));
        const auto end = parse_hex(
            std::string_view(line).substr(separator + 1, permissions_start - separator - 1));
        const auto permissions = std::string_view(line).substr(
            permissions_start + 1, permissions_end - permissions_start - 1);
        const auto name_start = line.find_first_not_of(' ', permissions_end);
        const auto name = name_start == std::string::npos
                              ? std::string{}
                              : trim_left(line.substr(name_start));
        if (end <= start) {
            throw std::runtime_error("invalid range in /proc maps");
        }
        state.apply(MmapEvent{start, end - start, protection_from_permissions(permissions), name});
    }
}

}  // namespace sys_sight
