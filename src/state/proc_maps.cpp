#include "state/proc_maps.h"

#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

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

}  // namespace

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

std::vector<ProcMapsEntry> parse_proc_maps(std::istream& maps) {
    std::vector<ProcMapsEntry> entries;
    std::string line;
    while (std::getline(maps, line)) {
        // start-end perms offset dev inode   pathname
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
        if (end <= start) {
            throw std::runtime_error("invalid range in /proc maps");
        }

        // Skip offset, dev and inode; the rest of the line is the pathname,
        // which may itself contain spaces.
        auto position = permissions_end;
        for (int field = 0; field < 3; ++field) {
            position = line.find_first_not_of(' ', position);
            if (position == std::string::npos) {
                throw std::runtime_error("missing fields in /proc maps");
            }
            position = line.find(' ', position);
            if (position == std::string::npos) {
                position = line.size();
            }
        }
        const auto name_start = line.find_first_not_of(' ', position);
        entries.push_back({start, end,
                           line.substr(permissions_start + 1, permissions_end - permissions_start - 1),
                           name_start == std::string::npos ? std::string{} : line.substr(name_start)});
    }
    return entries;
}

std::vector<ProcMapsEntry> read_proc_maps(pid_t pid) {
    const std::string maps_path = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream maps(maps_path);
    if (!maps) {
        throw std::system_error(errno, std::generic_category(), "failed to open " + maps_path);
    }
    return parse_proc_maps(maps);
}

std::string read_proc_exe(pid_t pid) {
    const std::string link_path = "/proc/" + std::to_string(pid) + "/exe";
    std::string target(4096, '\0');
    const auto length = readlink(link_path.c_str(), target.data(), target.size());
    if (length < 0) {
        return {};
    }
    target.resize(static_cast<std::size_t>(length));
    return target;
}

}  // namespace sys_sight
