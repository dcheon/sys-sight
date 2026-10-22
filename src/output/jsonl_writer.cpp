#include "output/jsonl_writer.h"

#include <ostream>

namespace sys_sight {

void write_jsonl(std::ostream& output, const SyscallEvent& event) {
    output << "{\"sequence\":" << event.sequence << ",\"pid\":" << event.pid
           << ",\"tid\":" << event.tid << ",\"syscall_number\":" << event.number
           << ",\"phase\":\"" << (event.entry ? "entry" : "exit")
           << "\",\"return_value\":" << event.return_value
           << ",\"timestamp_ns\":" << event.timestamp_ns << "}\n";
}

}  // namespace sys_sight
