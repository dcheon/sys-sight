#pragma once

#include "elf/elf_reader.h"
#include "state/memory_oracle.h"
#include "state/region_classifier.h"
#include "state/stack_walker.h"
#include "tracer/ptrace_tracer.h"

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace sys_sight {

// Version of the JSONL output format, documented in docs/OUTPUT_SCHEMA.md.
inline constexpr int kSchemaVersion = 1;

void write_jsonl(std::ostream& output, const SyscallEvent& event);

// Addresses are written as hex strings: JSON numbers lose precision above 2^53.
[[nodiscard]] std::string hex_address(std::uint64_t value);
void write_json_string(std::ostream& output, std::string_view value);
void write_regions_json(std::ostream& output, const std::vector<ClassifiedRegion>& regions);
void write_stack_json(std::ostream& output, const StackWalk& walk);
// `compared` is false when /proc/<pid>/maps could not be read, so there is no verdict.
void write_oracle_json(std::ostream& output, const OracleResult& oracle, bool compared,
                       std::size_t reducer_regions, std::size_t proc_regions);
void write_executable_json(std::ostream& output, const std::string& path, const ElfInfo& elf);

}  // namespace sys_sight
