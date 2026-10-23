#include "cli/run_source.h"
#include "output/jsonl_writer.h"
#include "tracer/ptrace_tracer.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int usage() {
    std::cerr << "usage: sys-sight <executable> [args...]\n"
                 "       sys-sight run <source.c> [args...]\n";
    return 2;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> arguments;
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    const bool run_source = !arguments.empty() && arguments.front() == "run";
    if (arguments.empty() || (run_source && arguments.size() < 2)) {
        return usage();
    }

    std::cout << "{\"schema_version\":" << sys_sight::kSchemaVersion << "}\n";
    if (run_source) {
        return sys_sight::run_source(arguments[1],
                                     std::vector<std::string>(arguments.begin() + 2, arguments.end()),
                                     std::cout);
    }
    return sys_sight::PtraceTracer(std::move(arguments)).run(std::cout);
}
