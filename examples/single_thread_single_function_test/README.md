# single_thread_single_function_test

A single-threaded C program that does everything in `main`: one `int` in each
region of the process's virtual address space, and a snapshot marker after
all of them are set up.

| Variable | Region |
| --- | --- |
| `g_rodata` | `.rodata` |
| `g_data` | `.data` |
| `g_bss` | `.bss` |
| `*heap_int` | heap (`malloc`, via `brk`) |
| `*mapped_int` | anonymous `mmap` region |
| `in_main` | stack, the `main` frame |

## Build and run

```bash
gcc -std=gnu11 -O0 -g -no-pie -fno-omit-frame-pointer \
    -Iinclude examples/single_thread_single_function_test/main.c -o single_thread_single_function_test
./single_thread_single_function_test
```

The program prints the address and value of each variable. Without a tracer,
`SYSSIGHT_SNAPSHOT()` has no effect.

## Planned: snapshot view

When run under `sys-sight`, the marker will capture the process state and
show it with low addresses on top:

```text
low addresses
  text     main
  rodata   g_rodata
  data     g_data
  bss      g_bss
  heap     *heap_int
  mmap     *mapped_int, shared libraries
  stack    main (in_main)
high addresses
```

The addresses printed by the program are the reference for checking the view.
