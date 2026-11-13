# SysSight

SysSight shows what a running C program looks like to the operating system.
It runs a program, stops it at a point you mark, and shows where its data
lives: which memory region each variable is in, what the stack frames hold,
and which system calls the program made. It exists because operating-system
topics such as memory layout, `fork`, and threads are easier to understand
when you can see them than when you only read about them.

Under the hood it is a C++20 Linux x86-64 system call tracer. It uses `ptrace`
to collect system call entry/exit events and pure state reducers to model file
descriptor and virtual memory mapping lifecycles. An oracle checks the memory
map rebuilt from system calls against `/proc/<pid>/maps`.

## Status

Works today, for a single process: memory regions, global variables, stack
frames (function names), system call events, and a local viewer.

Planned, in this order (details in [docs/ROADMAP.md](docs/ROADMAP.md)):
local variables, `fork` parent/child comparison, thread comparison with shared
regions, and changes over time.

## Quick start

```bash
cmake -S . -B build -DSYS_SIGHT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/sys-sight /bin/echo
```

The tracer currently targets Linux x86-64 and requires CMake 3.20 or newer
with a C++20 compiler.

## Run with Docker

The image builds the project and its tests on Ubuntu 24.04. `ctest` runs
inside the container when it starts.

```bash
docker build -t sys-sight .
docker run --rm sys-sight
```

All tests, including the ones that trace programs with `ptrace`, passed this
way on Docker Desktop (WSL2) without extra privileges. If `ptrace` fails with
`Operation not permitted` on another host, add `--cap-add=SYS_PTRACE`:

```bash
docker run --rm --cap-add=SYS_PTRACE sys-sight
```

## Snapshots and viewer

A program built with `-O0 -g -no-pie -fno-omit-frame-pointer` can mark the
point to inspect with `SYSSIGHT_SNAPSHOT()` from
[include/sys-sight/marker.h](include/sys-sight/marker.h). Under `sys-sight`,
the marker emits a snapshot of the process's memory regions, global variables,
and stack frames; without a tracer it has no effect.

```bash
./build/sys-sight ./a.out > output.jsonl
```

`sys-sight run` compiles a C source with those flags and traces it in one
step. The compiler is `$CC` or `cc`; extra arguments go to the program, and
the build files are removed afterwards.

```bash
./build/sys-sight run program.c [args...] > output.jsonl
```

Each snapshot also checks the memory map that `sys-sight` rebuilt from system
calls (`mmap`, `munmap`, `mprotect`, `mremap`, `brk`) against
`/proc/<pid>/maps` and reports any difference.

Open [viewer/index.html](viewer/index.html) in a browser and load
`output.jsonl` to see the address space with low addresses on top. The viewer
runs locally and needs no server.

Example target programs live in [examples/](examples/). Exit codes and errno
values are described in [docs/EXIT_CODES.md](docs/EXIT_CODES.md), and the
JSONL output format in [docs/OUTPUT_SCHEMA.md](docs/OUTPUT_SCHEMA.md).

See [docs/ROADMAP.md](docs/ROADMAP.md) for the architecture, implementation
plan, milestones, and definition of done.
