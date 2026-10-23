# Exit codes and errno

## What errno is

When a Linux system call fails, the kernel reports why with a small positive
number called an *errno*. C programs read it from the `errno` variable after a
failed call; a raw system call returns it negated (`-2` means `ENOENT`).

| errno | Name | Meaning |
| --- | --- | --- |
| 1 | `EPERM` | Operation not permitted |
| 2 | `ENOENT` | No such file or directory |
| 3 | `ESRCH` | No such process |
| 4 | `EINTR` | Interrupted system call |
| 10 | `ECHILD` | No child processes |
| 11 | `EAGAIN` | Resource temporarily unavailable |
| 12 | `ENOMEM` | Cannot allocate memory |
| 13 | `EACCES` | Permission denied |
| 22 | `EINVAL` | Invalid argument |
| 38 | `ENOSYS` | Function not implemented |

Numbers are for Linux x86-64. `strerror(n)` or `python3 -c "import os;
print(os.strerror(n))"` translates any number.

## In the JSONL output

- On an `exit` event, a `return_value` between `-4095` and `-1` is a failed
  call, and its negation is the errno. `"return_value":-2` is `ENOENT`.
- On an `entry` event, `return_value` is always `-38` (`ENOSYS`). The kernel
  has not produced a result yet, so the value carries no meaning.
- Failed calls (negative return values) do not change the file descriptor or
  memory map state.

## sys-sight exit codes

`sys-sight <executable> [args...]` exits with the traced program's result. The
last JSONL line says which case applies:

| Last line | Exit code | Meaning |
| --- | --- | --- |
| `{"exit":{"status":N}}` | `N` | The traced program exited with status `N` |
| `{"exit":{"signal":S}}` | `128 + S` | The traced program was killed by signal `S` (`139` is `SIGSEGV`) |
| `{"error":"bootstrap_failed",...}` | `1` | The initial `/proc` state could not be read |
| `{"error":"exec_failed","errno":N,...}` | `127` if not found, otherwise `126` | The executable could not be started; a message is also printed to stderr |
| none | an errno | A system call made by the tracer itself failed: `fork`, `waitpid`, or `ptrace` |
| `{"error":"compile_failed",...}`, `source_unreadable`, `unsupported_source`, or `build_failed` | `1` | `sys-sight run` could not build the source; no program was traced |
| none | `2` | No executable or source was given; usage is printed to stderr |

## Limitations

- Tracer errors and traced-program exit statuses share one number range. An
  exit code of `3` is `ESRCH` from the tracer only when there is no `exit`
  line; if there is one, the code belongs to the traced program.
- `126` and `127` are also ordinary exit statuses a traced program can
  return; the `exec_failed` line tells the two cases apart.

## Planned

- Report the remaining tracer errors (`fork`, `waitpid`, `ptrace` failures) in
  the `error` field of the JSON output, as `exec_failed` already does, so they
  cannot be confused with the traced program's exit status.
