#include "output/jsonl_writer.h"

#include <cstdio>
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

std::string hex_address(std::uint64_t value) {
    char buffer[19];
    std::snprintf(buffer, sizeof buffer, "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

void write_json_string(std::ostream& output, std::string_view value) {
    output << '"';
    for (const char character : value) {
        const auto code = static_cast<unsigned char>(character);
        if (character == '"' || character == '\\') {
            output << '\\' << character;
        } else if (code < 0x20) {
            char buffer[7];
            std::snprintf(buffer, sizeof buffer, "\\u%04x", code);
            output << buffer;
        } else {
            output << character;
        }
    }
    output << '"';
}

void write_regions_json(std::ostream& output, const std::vector<ClassifiedRegion>& regions) {
    output << '[';
    bool first = true;
    for (const auto& region : regions) {
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"start\":\"" << hex_address(region.entry.start) << "\",\"end\":\""
               << hex_address(region.entry.end) << "\",\"permissions\":\""
               << region.entry.permissions << "\",\"kind\":\"" << to_string(region.kind)
               << "\",\"name\":";
        write_json_string(output, region.entry.name);
        output << '}';
    }
    output << ']';
}

void write_executable_json(std::ostream& output, const std::string& path, const ElfInfo& elf) {
    output << "{\"path\":";
    write_json_string(output, path);
    output << ",\"pie\":" << (elf.pie ? "true" : "false") << ",\"sections\":[";
    for (std::size_t index = 0; index < elf.sections.size(); ++index) {
        const auto& section = elf.sections[index];
        output << (index == 0 ? "" : ",") << "{\"name\":";
        write_json_string(output, section.name);
        output << ",\"start\":\"" << hex_address(section.start) << "\",\"end\":\""
               << hex_address(section.end) << "\"}";
    }
    output << "],\"symbols\":[";
    for (std::size_t index = 0; index < elf.symbols.size(); ++index) {
        const auto& symbol = elf.symbols[index];
        output << (index == 0 ? "" : ",") << "{\"name\":";
        write_json_string(output, symbol.name);
        output << ",\"address\":\"" << hex_address(symbol.address) << "\",\"size\":" << symbol.size
               << ",\"section\":";
        write_json_string(output, symbol.section);
        if (symbol.value) {
            output << ",\"value\":\"" << hex_address(*symbol.value) << '"';
        }
        output << '}';
    }
    output << "]}";
}

}  // namespace sys_sight
