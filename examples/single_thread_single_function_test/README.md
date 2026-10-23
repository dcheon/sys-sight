# single_thread_single_function_test

A single-threaded C program that does everything in `main`: one `int` in each
region of the process's virtual address space, and a snapshot marker after
all of them are set up.

## Regions

A process sees one virtual address space, divided into regions. This program
places one variable in each region that applies to a simple C program.

| Region | Holds | Permissions | Lifetime | Variable here |
| --- | --- | --- | --- | --- |
| `.rodata` | Constants and string literals | read-only | whole run | `g_rodata` |
| `.data` | Global/static variables with a non-zero initial value | read-write | whole run | `g_data` |
| `.bss` | Global/static variables initialized to zero or not at all | read-write | whole run | `g_bss` |
| heap | Memory from `malloc`; grows upward from the program break (`brk`) | read-write | until `free` | `*heap_int` |
| `mmap` | Memory mapped with `mmap`, including shared libraries; large `malloc` calls can also land here | per mapping | until `munmap` | `*mapped_int` |
| stack | Local variables, saved registers, and return addresses of active function calls; grows downward | read-write | until the function returns | `in_main`, `heap_int`, `mapped_int` |

Code (`.text`) also has a region, but this program does not print a variable
from it.

On Linux x86-64 the regions sit in this order from low to high addresses:
`.text`, `.rodata`, `.data`, `.bss`, heap, `mmap`, stack.

A pointer variable lives in the region of the pointer itself, not of its
target. `heap_int` and `mapped_int` are on the stack, while the `int`s they
point to are on the heap and in the `mmap` region.

## Build and run

Run from the repository root, on Linux (or WSL).

```bash
gcc -std=gnu11 -O0 -g -no-pie -fno-omit-frame-pointer \
    -Iinclude examples/single_thread_single_function_test/main.c -o single_thread_single_function_test
./single_thread_single_function_test
```

The program prints a table with these columns:

| Column | Meaning |
| --- | --- |
| `name` | Variable name in the source |
| `value` | Value stored in the variable; for a pointer, the address it holds |
| `region` | Region the variable lives in |
| `address` | Address of the variable itself |
| `points to` | For a pointer, the variable at the address it holds; `-` otherwise |

This table is written by the program itself and serves as a reference. A
program does not need to print anything to be inspected: `sys-sight` is
planned to build the same view from the running process.

Without a tracer, `SYSSIGHT_SNAPSHOT()` has no effect.

Example output (heap, `mmap`, and stack addresses differ between runs unless
ASLR is disabled):

```text
name         value            region   address          points to
g_rodata     3                .rodata  0x402004         -
g_data       1                .data    0x404048         -
g_bss        0                .bss     0x404050         -
*heap_int    5                heap     0x28b17010       -
*mapped_int  6                mmap     0x72805e129000   -
in_main      4                stack    0x7ffcf95cf434   -
heap_int     0x28b17010       stack    0x7ffcf95cf438   *heap_int
mapped_int   0x72805e129000   stack    0x7ffcf95cf440   *mapped_int
```

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
