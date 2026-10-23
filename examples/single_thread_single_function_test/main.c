#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "sys-sight/marker.h"

#define SHOW(x) printf("%-12s %p = %d\n", #x, (void *)&(x), (x))

int g_data = 1;            /* .data */
int g_bss;                 /* .bss */
const int g_rodata = 3;    /* .rodata */

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

    SHOW(g_rodata);
    SHOW(g_data);
    SHOW(g_bss);
    SHOW(*heap_int);
    SHOW(*mapped_int);
    SHOW(in_main);

    free(heap_int);
    return 0;
}
