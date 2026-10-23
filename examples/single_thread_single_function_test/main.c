#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "sys-sight/marker.h"

int g_data = 1;            /* .data */
int g_bss;                 /* .bss */
const int g_rodata = 3;    /* .rodata */

static void row_int(const char *name, const char *region, const int *p)
{
    char value[32];

    snprintf(value, sizeof value, "%d", *p);
    printf("%-12s %-16s %-8s %-16p %s\n", name, value, region,
           (const void *)p, "-");
}

/* The value of a pointer is an address; `target` names what it points to. */
static void row_ptr(const char *name, const char *region, void *const *p,
                    const char *target)
{
    char value[32];

    snprintf(value, sizeof value, "%p", *p);
    printf("%-12s %-16s %-8s %-16p %s\n", name, value, region,
           (const void *)p, target);
}

int main(void)
{
    int in_main = 4;       /* stack */
    int *heap_int = malloc(sizeof *heap_int);
    int *mapped_int = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (!heap_int || mapped_int == MAP_FAILED)
        return 1;
    *heap_int = 5;
    *mapped_int = 6;

    SYSSIGHT_SNAPSHOT();

    printf("%-12s %-16s %-8s %-16s %s\n", "name", "value", "region",
           "address", "points to");
    row_int("g_rodata", ".rodata", &g_rodata);
    row_int("g_data", ".data", &g_data);
    row_int("g_bss", ".bss", &g_bss);
    row_int("*heap_int", "heap", heap_int);
    row_int("*mapped_int", "mmap", mapped_int);
    row_int("in_main", "stack", &in_main);
    row_ptr("heap_int", "stack", (void *const *)&heap_int, "*heap_int");
    row_ptr("mapped_int", "stack", (void *const *)&mapped_int, "*mapped_int");

    free(heap_int);
    return 0;
}
