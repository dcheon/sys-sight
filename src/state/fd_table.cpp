#include "state/fd_table.h"

#include <stdexcept>
#include <utility>

namespace sys_sight {

void FileDescriptorTable::clear() noexcept {
    entries_.clear();
}

void FileDescriptorTable::open(FileDescriptor fd, std::string path) {
    if (fd < 0) {
        throw std::invalid_argument("file descriptor must be non-negative");
    }
    entries_[fd] = std::move(path);
}

void FileDescriptorTable::close(FileDescriptor fd) {
    entries_.erase(fd);
}

void FileDescriptorTable::duplicate(FileDescriptor source, FileDescriptor target) {
    const auto source_entry = entries_.find(source);
    if (source_entry == entries_.end()) {
        throw std::invalid_argument("cannot duplicate an unknown file descriptor");
    }
    entries_[target] = source_entry->second;
}

bool FileDescriptorTable::contains(FileDescriptor fd) const {
    return entries_.contains(fd);
}

std::optional<std::string> FileDescriptorTable::path(FileDescriptor fd) const {
    const auto entry = entries_.find(fd);
    if (entry == entries_.end()) {
        return std::nullopt;
    }
    return entry->second;
}

std::size_t FileDescriptorTable::size() const noexcept {
    return entries_.size();
}

const std::map<FileDescriptor, std::string>& FileDescriptorTable::entries() const noexcept {
    return entries_;
}

}  // namespace sys_sight
