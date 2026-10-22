# SysSight

SysSight is a C++20 Linux x86-64 system call tracing and process-state
reconstruction engine. It collects system call entry/exit events with
`ptrace(PTRACE_SYSCALL)` and provides pure state reducers for file descriptor
and virtual memory mapping lifecycles.

## Current status

The repository currently contains the first Phase 1 vertical slice:

- C++20 and Modern CMake project setup
- Single-process system call tracing
- `PTRACE_O_TRACESYSGOOD`-based system call entry/exit classification
- JSON Lines event output
- A ptrace-independent `FileDescriptorTable`
- A ptrace-independent `VirtualMemoryMap`
- A `ProcessState` reducer that applies typed state events
- File descriptor open, duplicate, and close state transitions
- Virtual memory mapping and middle-range unmapping with region splitting
- GoogleTest unit tests
- GitHub Actions Linux CI with AddressSanitizer and UndefinedBehaviorSanitizer configurations

The tracer targets Linux x86-64. The project can be configured on Windows for
source inspection, but the tracer and its Linux-specific headers require Linux
or WSL to build and run.

## Architecture

```text
Target Process
      |
      v
PtraceTracer
      |
      v
Raw Syscall Events
      |
      v
ProcessState
  |         |
  v         v
FD Table   Virtual Memory Map
      |
      v
JSONL Output
```

The tracing layer is intentionally separate from the reducer layer. This
allows the state models to be tested without launching a traced process or
depending on `ptrace`.

## Requirements

- Linux x86-64
- C++20 compiler
- CMake 3.20 or newer
- Ninja, Make, or another supported CMake generator
- Internet access during the first test-enabled configure, unless GoogleTest
  is already available through the local CMake dependency cache

## Build and test

From the repository root:

```bash
cmake -S . -B build -DSYS_SIGHT_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

To configure only the tracer without downloading GoogleTest:

```bash
cmake -S . -B build -DSYS_SIGHT_BUILD_TESTS=OFF
cmake --build build --parallel
```

## Usage

Run the tracer with a target executable:

```bash
./build/sys-sight /bin/echo
```

The current command accepts an executable path as its only argument. It emits
one JSON object per line containing:

- Event sequence number
- Process and thread identifiers
- System call number
- `entry` or `exit` phase
- Register-derived return value
- Monotonic timestamp in nanoseconds

Example event:

```json
{
  "sequence": 0,
  "pid": 1234,
  "tid": 1234,
  "syscall_number": 59,
  "phase": "entry",
  "return_value": -38,
  "timestamp_ns": 123456789
}
```

At this stage, system call numbers are emitted as raw values. Symbolic
decoding, `/proc` bootstrap, and reducer integration with live system call
events are planned next.

## Roadmap

### Phase 1: Single-process state reconstruction

- Add x86-64 system call decoding
- Add `PTRACE_GET_SYSCALL_INFO`
- Bootstrap initial state from `/proc/<pid>/maps` and `/proc/<pid>/fd`
- Connect `openat`, `close`, `dup*`, `mmap`, `munmap`, `mremap`, `mprotect`,
  and `brk` events to the reducers
- Add `/proc`-based verification of reducer state
- Export state changes for Perfetto-compatible timelines

### Phase 2: Multi-threading and filtering

- Support `PTRACE_O_TRACECLONE`, `PTRACE_O_TRACEFORK`, and
  `PTRACE_O_TRACEEXEC`
- Build a `waitpid(-1, ..., __WALL)` multi-thread event loop
- Track per-thread system call state
- Model shared and separate FD/VMA resources
- Add optional seccomp-BPF filtering

### Phase 3: Benchmarking and documentation

- Measure baseline versus traced workloads
- Report wall-clock time, CPU time, peak RSS, event throughput, and system
  call counts
- Publish Perfetto screenshots, architecture diagrams, and benchmark results

## Project layout

```text
sys-sight/
├── .github/workflows/ci.yml
├── CMakeLists.txt
├── src/
│   ├── cli/
│   ├── common/
│   ├── output/
│   ├── state/
│   └── tracer/
└── tests/unit/
```

## License

No license has been selected yet.
