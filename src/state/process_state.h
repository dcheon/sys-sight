#pragma once

#include "state/fd_table.h"
#include "state/memory_map.h"

namespace sys_sight {

class ProcessState {
public:
    void apply(const StateEvent& event);

    [[nodiscard]] const FileDescriptorTable& file_descriptors() const noexcept;
    [[nodiscard]] const VirtualMemoryMap& memory_map() const noexcept;

private:
    FileDescriptorTable file_descriptors_;
    VirtualMemoryMap memory_map_;
};

}  // namespace sys_sight
