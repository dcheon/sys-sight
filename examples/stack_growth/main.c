#include "sys-sight/marker.h"

/* Recurses until the stack is several times its initial size, then marks a
 * snapshot. The kernel grows the stack on page faults, with no system call. */
static int deep(int levels)
{
    volatile char buffer[4096];

    buffer[0] = (char)levels;
    buffer[4095] = 1;
    if (levels > 0) {
        return deep(levels - 1) + buffer[0] + buffer[4095];
    }
    SYSSIGHT_SNAPSHOT();
    return 0;
}

int main(void)
{
    return deep(150) < 0;
}
