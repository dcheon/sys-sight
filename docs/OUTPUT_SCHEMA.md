# Output format

`sys-sight` writes JSON Lines to stdout: one JSON object per line. Every line
has a single top-level key that names its kind. The format version is
`schema_version`, currently `1`.

## Versioning

- The first line of every run is `{"schema_version":1}`.
- Adding fields or line kinds does not change the version. Readers must ignore
  fields and line kinds they do not know.
- The version increases when the meaning or type of an existing field changes,
  or when a field is removed. Readers should refuse a version they do not know.

## Conventions

- Addresses in snapshots and diagnostics are hex strings such as `"0x404048"`,
  because JSON numbers lose precision above 2^53. Syscall event `arguments`
  are plain decimal numbers.
- Sizes and counts are plain numbers.
- Optional fields are omitted when they have no value, unless noted as `null`.

## Line kinds

| Key | Meaning |
| --- | --- |
| `schema_version` | Format version. Always the first line. |
| `bootstrap` | Counts of file descriptors and memory regions found at start. |
| `sequence` | A system call event (see below). |
| `snapshot` | The process state at a snapshot marker. |
| `diagnostic` | A warning about the quality of the result. |
| `error` | Something could not be read or started. |
| `exit` | The traced program ended. Always the last line of a run that started. |

### System call event

```json
{"sequence":12,"pid":4242,"tid":4242,"syscall_number":9,"syscall":"mmap","phase":"exit","arguments":[0,4096,3,34,4294967295,0],"return_value":140737488347136,"timestamp_ns":110238379096844}
```

`phase` is `entry` or `exit`. `syscall` is `unknown` for numbers without a known
name. `return_value` is only meaningful on `exit`; see
[EXIT_CODES.md](EXIT_CODES.md).

### Snapshot

Emitted when the traced program calls `SYSSIGHT_SNAPSHOT()`.

```json
{"snapshot":{
  "trigger":"marker",
  "sequence":64,
  "registers":{"rip":"0x7b6f9f334acd","rsp":"0x7ffd2d407988","rbp":"0x7ffd2d4079b0"},
  "regions":[{"start":"0x401000","end":"0x402000","permissions":"r-xp","kind":"text","name":"/path/to/program"}],
  "executable":{
    "path":"/path/to/program",
    "pie":false,
    "sections":[{"name":".data","start":"0x404038","end":"0x40404c"}],
    "symbols":[{"name":"g_data","address":"0x404048","size":4,"section":".data","value":"0x1"}]
  },
  "stack":{
    "end":"left_executable",
    "frames":[{"index":0,"function":"main","start":"0x7ffd2d407990","end":"0x7ffd2d4079c0",
               "frame_pointer":"0x7ffd2d4079b0","saved_frame_pointer":"0x1","return_address":"0x7b6f9f22a601"}]
  }
}}
```

| Field | Meaning |
| --- | --- |
| `trigger` | What caused the snapshot. Only `marker` exists. |
| `sequence` | Sequence number of the marker's system call event. |
| `registers` | `rip`, `rsp`, `rbp` at the marker. `rsp` is inside the C library's `syscall()` wrapper, 8 bytes below the calling function's stack pointer. |
| `regions` | The process's memory mappings, from `/proc/<pid>/maps` at snapshot time, in address order. |
| `executable.pie` | Whether the program is position-independent. If `true`, `sections` and `symbols` are empty. |
| `executable.sections` | Loaded sections with their address ranges. This is how `.data` and `.bss` are told apart. |
| `executable.symbols` | Data objects from the symbol table, including toolchain-generated ones. `value` is present for objects of 1, 2, 4 or 8 bytes, as a hex string of the little-endian value. The list is empty for stripped programs. |
| `stack.frames` | Stack frames from the innermost (the function that called the marker, index 0) outward. `start` to `end` is the frame's address range, `function` is `null` when unknown. |
| `stack.end` | Why the walk stopped (see below). |

Region `kind`:

| Value | Meaning |
| --- | --- |
| `text` | The program's executable code. |
| `rodata` | The program's read-only data. |
| `data` | The program's writable segment (`.data` and `.bss`). |
| `heap` | The `[heap]` mapping (`brk`). |
| `mmap` | Anonymous mappings and shared libraries. |
| `stack` | The `[stack]` mapping. |
| `kernel` | `[vdso]`, `[vvar]` and similar. |

`stack.end`:

| Value | Meaning |
| --- | --- |
| `left_executable` | The next caller is outside the program (normally the C library calling `main`). |
| `broken_chain` | The saved frame pointers are unreadable or inconsistent. |
| `no_frame_pointer` | A function does not set up a frame pointer; the walk stopped before it. |
| `depth_limit` | 64 frames were reached. |

### Diagnostic

```json
{"diagnostic":{"level":"warning","code":"no_frame_pointer","message":"..."}}
```

| `code` | Meaning |
| --- | --- |
| `pie` | The program is position-independent; sections and symbols are omitted. |
| `stack_walk` | No frames, or a broken frame pointer chain. |
| `no_frame_pointer` | A function was built without a frame pointer. |

Diagnostics for a snapshot appear before its `snapshot` line.

### Error

```json
{"error":"exec_failed","errno":2,"message":"No such file or directory"}
```

| `error` | Meaning |
| --- | --- |
| `exec_failed` | The program could not be started. |
| `bootstrap_failed` | The initial `/proc` state could not be read. |
| `maps_unreadable` | `/proc/<pid>/maps` could not be read for a snapshot. |
| `elf_unreadable` | The program's ELF file could not be read for a snapshot. |

### Exit

`{"exit":{"status":0}}` for a normal exit, `{"exit":{"signal":11}}` when the
program was killed by a signal. See [EXIT_CODES.md](EXIT_CODES.md) for the
process exit code.

## Planned

- A `trigger` of `breakpoint` with a file and line.
- Several snapshots per run.
- Source file and line for each stack frame, and local variable names.
- Position-independent executables.
