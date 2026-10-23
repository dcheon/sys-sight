#include <stdio.h>

#include "sys-sight/marker.h"

int g_data = 7;            /* .data */
int g_bss;                 /* .bss */

static void row_int(const char *name, const char *region, const int *p)
{
    char value[32];

    snprintf(value, sizeof value, "%d", *p);
    printf("%-12s %-16s %-8s %-16p %s\n", name, value, region, (const void *)p, "-");
}

static void inner(void)
{
    int in_inner = 3;      /* stack, innermost frame */

    SYSSIGHT_SNAPSHOT();

    row_int("in_inner", "stack", &in_inner);
}

static void outer(void)
{
    int in_outer = 2;      /* stack, middle frame */

    inner();
    row_int("in_outer", "stack", &in_outer);
}

int main(void)
{
    int in_main = 1;       /* stack, outermost frame */

    printf("%-12s %-16s %-8s %-16s %s\n", "name", "value", "region", "address", "points to");
    outer();
    row_int("in_main", "stack", &in_main);
    row_int("g_data", ".data", &g_data);
    row_int("g_bss", ".bss", &g_bss);
    return 0;
}
