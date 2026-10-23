#include "state/process_state.h"

#include <type_traits>
#include <variant>

namespace sys_sight {

void ProcessState::clear() noexcept {
    file_descriptors_.clear();
    memory_map_.clear();
}

void ProcessState::seed_standard_descriptors() {
    file_descriptors_.open(0, "<stdin>");
    file_descriptors_.open(1, "<stdout>");
    file_descriptors_.open(2, "<stderr>");
}

void ProcessState::apply(const StateEvent& event) {
    std::visit(
        [this](const auto& value) {
            using Event = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Event, OpenEvent>) {
                file_descriptors_.open(value.fd, value.path);
            } else if constexpr (std::is_same_v<Event, CloseEvent>) {
                file_descriptors_.close(value.fd);
            } else if constexpr (std::is_same_v<Event, DupEvent>) {
                file_descriptors_.duplicate(value.source, value.target);
            } else if constexpr (std::is_same_v<Event, MmapEvent>) {
                memory_map_.map(value.start, value.length, value.protection, value.name);
            } else if constexpr (std::is_same_v<Event, MunmapEvent>) {
                memory_map_.unmap(value.start, value.length);
            } else if constexpr (std::is_same_v<Event, BrkEvent>) {
                memory_map_.set_break(value.requested, value.current_break);
            } else if constexpr (std::is_same_v<Event, MprotectEvent>) {
                memory_map_.protect(value.start, value.length, value.protection);
            } else if constexpr (std::is_same_v<Event, MremapEvent>) {
                memory_map_.remap(value.old_start, value.old_length, value.new_start,
                                  value.new_length, value.keep_old);
            }
        },
        event);
}

const FileDescriptorTable& ProcessState::file_descriptors() const noexcept {
    return file_descriptors_;
}

const VirtualMemoryMap& ProcessState::memory_map() const noexcept {
    return memory_map_;
}

}  // namespace sys_sight
