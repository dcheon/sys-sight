# SysSight Roadmap

## Project goal

SysSight aims to reconstruct the lifecycle of a Linux process from raw system
call events rather than producing only a linear text log. The initial target
is Linux x86-64 with a focus on file descriptor state and virtual memory
mapping state.

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
JSONL / Perfetto Export
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
- Add `PTRACE_GET_SYSCALL_INFO` support
- Decode the Linux x86-64 system call ABI
- Emit raw system call events as JSON Lines

### Bootstrap and state reduction

- Seed initial state from `/proc/<pid>/maps` and `/proc/<pid>/fd`
- Track file descriptor lifecycle events:
  `open`, `openat`, `openat2`, `close`, `dup*`, `fcntl` duplication,
  `pipe*`, `socket*`, `accept*`, `epoll_create*`, `eventfd*`,
  `memfd_create`, `close_range`, and `SCM_RIGHTS`
- Track virtual memory lifecycle events:
  `mmap`, `munmap`, `mremap`, `mprotect`, and `brk`
- Support region splitting and normalization after partial unmaps
- Reset the address-space model after `execve`

### Verification and export

- Compare reducer state with `/proc/<pid>/maps` and `/proc/<pid>/fd`
- Define explicit handling for guard pages and dynamically growing stacks
- Export state counters and diffs in a Perfetto-compatible format

## Phase 2: Multi-threading and seccomp filtering

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

### Phase 2

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
