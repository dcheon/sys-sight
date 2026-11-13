# SysSight Roadmap

## Project goal

SysSight makes operating-system concepts visible. It shows how a program's
variables sit in memory and how `fork` and threads change that picture, so
that a reader can see what a course describes in words.

To keep the picture correct, it reconstructs the lifecycle of a Linux process
from raw system call events instead of producing only a linear text log, and
checks the result against `/proc`. The initial target is Linux x86-64 with a
focus on virtual memory mapping state and file descriptor state.

## Priorities

The features below are ordered. Each one needs the ones before it.

1. **Syscall stop classification with `PTRACE_GET_SYSCALL_INFO`.** The tracer
   currently flips an in-syscall flag at every stop. That cannot be kept per
   process or thread, so every later item depends on replacing it.
2. **Local variables.** Show the local variables of each stack frame (name,
   type, address, value) inside the stack region. This needs debug
   information (DWARF). Every comparison below is about variables, so it comes
   before them.
3. **`fork` comparison.** Trace parent and child, snapshot both, and show them
   side by side: identical at the fork, diverging after a write.
4. **Thread comparison.** Show the threads of one process side by side, with
   globals, heap, and mappings marked as shared and each stack as private.
5. **Change over time.** Several snapshots per run and the difference between
   them, to follow a variable as the program runs.

Not needed for the goals above, so later: the full file descriptor system call
set, `execve` state reset, Perfetto export, benchmarks, seccomp filtering, and
ThreadSanitizer.

## Architecture

```text
Target Process
      |
      v
PtraceTracer
      |
      v
Syscall Decoder
      |
      v
ProcessStateReducer
  |             |
  v             v
FD Table     Virtual Memory Map
      |
      v
JSONL Export (Perfetto later)
      |
      v
Viewer
```

The tracer layer owns Linux and `ptrace` integration. The reducer layer must
remain independent of `ptrace` so it can be tested with deterministic typed
events.

## Phase 1: Single-process tracer and state reducer

### Infrastructure

- Modern CMake and C++20
- GoogleTest unit tests
- Google Benchmark performance tests
- Sanitizer builds with AddressSanitizer and UndefinedBehaviorSanitizer
- GitHub Actions Linux CI

### Tracing

- Collect single-process system call entry and exit events
- Use `PTRACE_O_TRACESYSGOOD` for stop classification
- [ ] Add `PTRACE_GET_SYSCALL_INFO` support
- [x] Decode the initial Linux x86-64 system call set
- [x] Emit decoded system call events as JSON Lines

### Bootstrap and state reduction

- [x] Seed the initial state from `/proc/<pid>/maps` and `/proc/<pid>/fd`
- [x] Seed standard descriptors as part of the initial single-process state
- [ ] Track the complete file descriptor lifecycle:
  `open`, `openat`, `openat2`, `close`, `dup*`, `fcntl` duplication,
  `pipe*`, `socket*`, `accept*`, `epoll_create*`, `eventfd*`,
  `memfd_create`, `close_range`, and `SCM_RIGHTS`
- [ ] Track the complete virtual memory lifecycle:
  `mmap`, `munmap`, `mremap`, `mprotect`, and `brk`
- [x] Support region splitting and normalization after partial unmaps
- [ ] Reset the address-space model after `execve`
- [x] Connect successful decoded syscall exits to the process state reducer

### Verification and export

- [x] Compare the reducer's memory map with `/proc/<pid>/maps` at each snapshot
- [ ] Compare the reducer's file descriptors with `/proc/<pid>/fd`
- [x] Define explicit handling for guard pages and dynamically growing stacks
- Export state counters and diffs in a Perfetto-compatible format (later)

### Snapshots and viewer

- [x] `SYSSIGHT_SNAPSHOT()` marker that emits memory regions, global
  variables, and stack frames
- [x] Read global variables from the ELF symbol table and the tracee's memory
- [x] Walk stack frames through frame pointers
- [x] `sys-sight run` to compile and trace a C source in one step
- [x] Local viewer for the JSONL output
- [ ] Local variables of each stack frame, from DWARF debug information
- [ ] Several snapshots per run
- [ ] Source file and line for each stack frame
- [ ] Position-independent executables

## Phase 2: Processes, threads, and seccomp filtering

- Trace children created by `fork` and `vfork` (`PTRACE_O_TRACEFORK`,
  `PTRACE_O_TRACEVFORK`) and start each child with a copy of its parent's state
- Identify the process in every snapshot and compare parent and child in the
  viewer
- Handle `PTRACE_O_TRACECLONE`, `PTRACE_O_TRACEFORK`, and
  `PTRACE_O_TRACEEXEC`
- Implement a `waitpid(-1, ..., __WALL)` event loop
- Maintain syscall state independently for every TID
- Model `CLONE_FILES` FD sharing and independent FD tables
- Model `CLONE_VM` address-space sharing
- Add optional seccomp-BPF filtering for relevant system calls
- Verify multi-threaded workloads with synthetic pthread targets
- Add ThreadSanitizer validation for the concurrent pipeline

## Phase 3: Benchmarking and documentation

- Compare baseline and traced execution
- Measure wall-clock time, CPU time, peak RSS, event throughput, and system
  call counts
- Compare tracing overhead with and without seccomp-BPF filtering
- Publish Perfetto screenshots and architecture diagrams
- Document benchmark workloads, hardware, kernel, compiler, and limitations

## Definition of done

### Phase 1

- Fifteen single-threaded synthetic workloads produce zero unexpected
  reducer/oracle mismatches
- FD and VMA reducer behavior is covered by deterministic unit tests
- The project builds and tests in Linux CI with normal, ASan, and UBSan
  configurations
- A snapshot of an example program lists its local variables with the correct
  name, address, and value

### Phase 2

- A synthetic `fork` workload produces zero unexpected oracle mismatches for
  both parent and child, and the viewer shows the two side by side
- Synthetic pthread workloads produce zero unexpected oracle mismatches
- Per-thread syscall state remains correct across clone and exit events
- Shared and independent resource semantics are tested

### Phase 3

- Benchmark results are reproducible from documented commands
- Performance claims include the workload and environment used for measurement
- Perfetto exports can be opened and inspected with the documented workflow

## Scope and limitations

The project tracks virtual address-space mappings, not physical memory
residency or allocator-internal usage. `mmap` and `brk` events cannot by
themselves reconstruct RSS, page faults, or all `malloc` activity.

The first implementation targets Linux x86-64 only. Other architectures,
attach mode, privileged tracing scenarios, and eBPF integration are outside
the initial Phase 1 scope.
