# SysSight

SysSight is a C++20 Linux x86-64 system call tracing and process-state
reconstruction engine.

The project uses `ptrace` to collect system call entry/exit events and pure
state reducers to model file descriptor and virtual memory mapping lifecycles.
The current implementation is an early Phase 1 vertical slice with a
single-process tracer, JSONL output, and unit-tested FD/VMA state models.

## Quick start

```bash
cmake -S . -B build -DSYS_SIGHT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/sys-sight /bin/echo
```

The tracer currently targets Linux x86-64 and requires CMake 3.20 or newer
with a C++20 compiler.

See [docs/ROADMAP.md](docs/ROADMAP.md) for the architecture, implementation
plan, milestones, and definition of done.
