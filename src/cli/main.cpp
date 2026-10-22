#include "tracer/ptrace_tracer.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: sys-sight <executable> [args...]\n";
        return 2;
    }
    std::vector<std::string> command;
    for (int index = 1; index < argc; ++index) {
        command.emplace_back(argv[index]);
    }
    return sys_sight::PtraceTracer(std::move(command)).run(std::cout);
}
