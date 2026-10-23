# SysSight Project Memory

This file is the persistent project context for SysSight. Update it when an
architecture decision, implementation milestone, validation result, or next
step changes. Use it as the first project-context document before continuing
development.

## Project identity

- **Name:** SysSight
- **Purpose:** Reconstruct Linux process state from system call events.
- **Primary target:** Linux x86-64
- **Language:** C++20
- **Current working branch:** `dev`
- **Stable branch:** `main`
- **Repository:** <https://github.com/dcheon/sys-sight>

## Core design

```text
Target Process
      |
      v
PtraceTracer
      |
      v
Syscall Event + x86-64 Decoder
      |
      v
ProcessState Reducer
  |             |
  v             v
FD Table     Virtual Memory Map
      |
      v
JSONL / Perfetto Export
```

The tracer layer owns Linux and `ptrace` integration. The reducer layer must
remain independent of `ptrace` and operate on deterministic typed events.
State changes are applied only for successful syscall exit events.

## Current implementation

### Tracing

- Single-process `fork` and `exec` launch mode
- `PTRACE_TRACEME`
- `PTRACE_SYSCALL`
- `PTRACE_O_TRACESYSGOOD`
- Syscall entry/exit alternation
- Linux x86-64 register extraction
- Tracee executable arguments are supported

### Decoding and reduction

- Initial syscall name decoding for:
  `read`, `write`, `close`, `dup`, `dup2`, `dup3`, `openat`, `mmap`,
  `munmap`, `mprotect`, `mremap`, `brk`, `execve`, `exit`, and `exit_group`
- Successful exit conversion to typed state events for:
  `openat`, `close`, `dup*`, `mmap`, `munmap`, and `brk`
- `FileDescriptorTable` with open, close, duplicate, clear, lookup, and size
- `VirtualMemoryMap` with mapping, unmapping, splitting, normalization,
  program-break tracking, and clear
- `ProcessState` reducer
- Initial state bootstrap from:
  - `/proc/<pid>/fd`
  - `/proc/<pid>/maps`
- Bootstrap result reports initial FD and VMA counts
- JSONL output includes syscall number, name, phase, arguments, return value,
  pid, tid, sequence, and timestamp

### Tests and automation

- GoogleTest unit tests for FD state, VMA splitting, process state, syscall
  decoding, and `/proc` bootstrap
- CMake test discovery through CTest
- GitHub Actions Linux CI on `ubuntu-24.04`
- CI matrix:
  - normal build
  - AddressSanitizer
  - UndefinedBehaviorSanitizer
- CI fails if no tests are discovered

## Important decisions and limitations

1. **Linux x86-64 first.** Do not add portability abstractions before the
   single-architecture behavior is correct and tested.
2. **Virtual memory is not physical memory.** The reducer models virtual
   mappings, not RSS, page residency, page faults, or allocator internals.
3. **Reducer purity matters.** Do not add `ptrace`, `/proc`, or process-global
   side effects to reducer classes.
4. **Apply only successful exits.** Entry events and negative syscall returns
   must not mutate FD or VMA state.
5. **Bootstrap is a seed, not an oracle.** `/proc` bootstrap establishes the
   initial model; a separate verification oracle is still required.
6. **Do not claim complete FD or VMA tracking yet.** Many lifecycle syscalls
   remain unsupported.
7. **Do not claim low overhead yet.** Performance claims require documented
   benchmark workloads and measurements.

## Completed milestones

- [x] Initial C++20 CMake project
- [x] GitHub repository and `main` branch
- [x] English README and roadmap
- [x] `dev` branch created and published
- [x] Single-process ptrace syscall event loop
- [x] Initial x86-64 syscall decoder
- [x] Successful syscall exit to reducer connection
- [x] `/proc` FD and VMA bootstrap
- [x] Unit-test CI matrix with sanitizers
- [x] Linux GCC syntax check and tracer smoke test under WSL

Latest development commit:

- `7e45aaf` — `Run unit tests in CI matrix`

Previous implementation commits:

- `753f1af` — `Add proc state bootstrap on dev`
- `2270b3f` — `Implement Phase 1 syscall reduction slice`

## Immediate next actions

Work on these in order:

1. Add `PTRACE_GET_SYSCALL_INFO` with a clear fallback or explicit Linux
   capability requirement.
2. Replace the global entry/exit toggle with per-TID syscall state, even before
   full multi-thread support.
3. Add safe tracee memory reads for syscall paths and metadata.
4. Decode syscall arguments needed by `openat`, including the path.
5. Expand FD lifecycle handling:
   `open`, `openat2`, `fcntl` duplication, `pipe*`, `socket*`, `accept*`,
   `epoll_create*`, `eventfd*`, `memfd_create`, and `close_range`.
6. Expand VMA handling for `mprotect`, `mremap`, `brk` growth/shrink, and
   overlapping or fixed mappings.
7. Reset and reseed state after successful `execve`.
8. Add deterministic integration workloads for FD, mmap, brk, and exec.
9. Implement a `/proc` verification oracle with detailed mismatch records.
10. Add Perfetto-compatible export only after reducer/oracle behavior is
    stable.

## Future phases

### Phase 2

- Multi-TID `waitpid(-1, ..., __WALL)` event loop
- `PTRACE_O_TRACECLONE`, `PTRACE_O_TRACEFORK`, and `PTRACE_O_TRACEEXEC`
- Per-thread syscall state
- `CLONE_FILES` and `CLONE_VM` resource semantics
- Optional seccomp-BPF filtering
- pthread integration workloads
- ThreadSanitizer validation of the concurrent pipeline

### Phase 3

- Baseline versus traced benchmark suite
- Wall-clock, CPU time, peak RSS, event throughput, and syscall counts
- Seccomp filtering overhead comparison
- Perfetto screenshots and reproducible benchmark documentation

## Working rules for future changes

- Make changes on `dev`; merge to `main` only after CI and review.
- Keep platform-specific code in tracer/bootstrap modules.
- Keep reducer tests runnable without a live tracee.
- Add or update a focused test for every state transition.
- Update this memory file when the current state or next actions change.
- Record measured results with workload, kernel, compiler, and environment.
- Keep README concise; place plans and historical context in `docs/`.
