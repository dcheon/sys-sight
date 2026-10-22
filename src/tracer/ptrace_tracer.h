#pragma once

#include "common/types.h"

#include <cstdint>
#include <ostream>
#include <string>

namespace sys_sight {

struct SyscallEvent {
    std::int64_t sequence;
    std::int32_t pid;
    std::int32_t tid;
    std::int64_t number;
    bool entry;
    std::int64_t return_value;
    std::uint64_t timestamp_ns;
};

class PtraceTracer {
public:
    explicit PtraceTracer(std::string executable);
    int run(std::ostream& output);

private:
    std::string executable_;
};

}  // namespace sys_sight
