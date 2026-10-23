#pragma once

#include "common/types.h"
#include "tracer/ptrace_tracer.h"

#include <optional>
#include <string_view>

namespace sys_sight {

[[nodiscard]] std::string_view syscall_name_x86_64(std::int64_t number) noexcept;
[[nodiscard]] std::optional<StateEvent> state_event_from_syscall(const SyscallEvent& event);

}  // namespace sys_sight
