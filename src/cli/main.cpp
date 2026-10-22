#include "tracer/ptrace_tracer.h"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: sys-sight <executable>\n";
        return 2;
    }
    return sys_sight::PtraceTracer(argv[1]).run(std::cout);
}
