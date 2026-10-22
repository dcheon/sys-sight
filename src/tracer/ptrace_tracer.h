#pragma once

#include "common/types.h"

#include <cstdint>
#include <array>
#include <ostream>
#include <string>
#include <vector>

namespace sys_sight {

struct SyscallEvent {
    std::int64_t sequence;
    std::int32_t pid;
    std::int32_t tid;
    std::int64_t number;
    bool entry;
    SyscallArguments arguments;
    std::int64_t return_value;
    std::uint64_t timestamp_ns;
    std::string name;
};

class PtraceTracer {
public:
    explicit PtraceTracer(std::vector<std::string> command);
    int run(std::ostream& output);

private:
    std::vector<std::string> command_;
};

}  // namespace sys_sight
