#include "output/jsonl_writer.h"

#include <ostream>

namespace sys_sight {

void write_jsonl(std::ostream& output, const SyscallEvent& event) {
    output << "{\"sequence\":" << event.sequence << ",\"pid\":" << event.pid
           << ",\"tid\":" << event.tid << ",\"syscall_number\":" << event.number
           << ",\"syscall\":\"" << event.name
           << "\",\"phase\":\"" << (event.entry ? "entry" : "exit")
           << "\",\"arguments\":[" << event.arguments[0] << "," << event.arguments[1] << ","
           << event.arguments[2] << "," << event.arguments[3] << "," << event.arguments[4]
           << "," << event.arguments[5]
           << "],\"return_value\":" << event.return_value
           << ",\"timestamp_ns\":" << event.timestamp_ns << "}\n";
}

}  // namespace sys_sight
