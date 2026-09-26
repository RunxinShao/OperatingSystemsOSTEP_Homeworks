// Chapter 6, measurement homework -- part 2: the cost of a system call.
//
// We time a loop of zero-byte read() calls on a valid descriptor. A 0-byte
// read does no work in the file system, so what we measure is the round trip
// into the kernel and back: trap, save state, dispatch, return, restore.
// A pure user-space loop of the same shape is timed as a control, so the
// loop overhead itself can be subtracted out.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define ITERS 1000000

static double now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

int main(int argc, char *argv[]) {
    int iters = (argc > 1) ? atoi(argv[1]) : ITERS;

    int fd = open("/dev/zero", O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    char buf[1];
    ssize_t r = 0;

    // warm up: fault in pages, prime the caches, settle the CPU frequency
    for (int i = 0; i < 10000; i++)
        r += read(fd, buf, 0);

    double t0 = now_us();
    for (int i = 0; i < iters; i++)
        r += read(fd, buf, 0);
    double t1 = now_us();
    if (r != 0)                         // never true; keeps the calls from being elided
        fprintf(stderr, "unexpected read result\n");

    // control loop: same iteration count, no system call
    volatile int sink = 0;
    double t2 = now_us();
    for (int i = 0; i < iters; i++)
        sink += i;
    double t3 = now_us();

    double syscall_ns = (t1 - t0) * 1000.0 / iters;
    double loop_ns    = (t3 - t2) * 1000.0 / iters;

    printf("iterations              : %d\n", iters);
    printf("total time for reads    : %.0f us\n", t1 - t0);
    printf("per zero-byte read()    : %.1f ns\n", syscall_ns);
    printf("empty control loop      : %.1f ns per iteration\n", loop_ns);
    printf("estimated syscall cost  : %.1f ns\n", syscall_ns - loop_ns);
    close(fd);
    return 0;
}
