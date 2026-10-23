#ifndef SYSSIGHT_MARKER_H
#define SYSSIGHT_MARKER_H

#include <sys/syscall.h>
#include <unistd.h>

/* Not a real Linux syscall number. Untraced, the kernel returns -ENOSYS and
 * the program continues; under sys-sight it requests a snapshot. */
#define SYSSIGHT_MARKER_SYSCALL 0x5353

#define SYSSIGHT_SNAPSHOT() ((void)syscall(SYSSIGHT_MARKER_SYSCALL))

#endif
