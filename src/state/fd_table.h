#pragma once

#include "common/types.h"

#include <map>
#include <optional>
#include <string>

namespace sys_sight {

class FileDescriptorTable {
public:
    void open(FileDescriptor fd, std::string path);
    void close(FileDescriptor fd);
    void duplicate(FileDescriptor source, FileDescriptor target);

    [[nodiscard]] bool contains(FileDescriptor fd) const;
    [[nodiscard]] std::optional<std::string> path(FileDescriptor fd) const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::map<FileDescriptor, std::string>& entries() const noexcept;

private:
    std::map<FileDescriptor, std::string> entries_;
};

}  // namespace sys_sight
