#pragma once

#include "tracer/ptrace_tracer.h"

#include <ostream>

namespace sys_sight {

void write_jsonl(std::ostream& output, const SyscallEvent& event);

}  // namespace sys_sight
