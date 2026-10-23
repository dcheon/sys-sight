#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "sys-sight/marker.h"

/* Exercises every system call that changes the address space, then marks a
 * snapshot. The addresses are not printed: the check compares the memory map
 * rebuilt from system calls with /proc/<pid>/maps. */
int main(void)
{
    const long page = sysconf(_SC_PAGESIZE);
    const int rw = PROT_READ | PROT_WRITE;
    const int flags = MAP_PRIVATE | MAP_ANONYMOUS;

    /* Changing the protection of part of a mapping splits it. */
    char *protect = mmap(NULL, 3 * page, rw, flags, -1, 0);
    mprotect(protect + page, page, PROT_READ);

    /* A length that is not a multiple of the page size is rounded up. */
    char *rounded = mmap(NULL, page + 1, rw, flags, -1, 0);

    /* MAP_FIXED replaces part of an existing mapping. */
    char *fixed = mmap(NULL, 4 * page, rw, flags, -1, 0);
    mmap(fixed + page, page, PROT_READ, flags | MAP_FIXED, -1, 0);

    /* mremap may grow in place or move, and can shrink. */
    char *moved = mmap(NULL, page, rw, flags, -1, 0);
    moved = mremap(moved, page, 4 * page, MREMAP_MAYMOVE);
    moved = mremap(moved, 4 * page, 2 * page, 0);

    /* The program break grows and shrinks. */
    sbrk(5 * page);
    sbrk(-2 * page);

    /* Unmapping the middle of a mapping splits it. This comes last among the
     * mappings so that no later mmap reuses the hole and hides the effect. */
    char *split = mmap(NULL, 4 * page, rw, flags, -1, 0);
    munmap(split + page, page);

    /* Small allocations come from the heap, large ones from mmap. */
    void *small = malloc(1000);
    void *large = malloc(1 << 20);

    SYSSIGHT_SNAPSHOT();

    free(large);
    free(small);
    return split == MAP_FAILED || protect == MAP_FAILED || rounded == MAP_FAILED ||
           fixed == MAP_FAILED || moved == MAP_FAILED;
}
