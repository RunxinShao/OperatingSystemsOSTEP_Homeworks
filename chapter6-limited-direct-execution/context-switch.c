// Chapter 6, measurement homework -- part 3: the cost of a context switch.
//
// The lmbench trick: two processes pinned to the SAME CPU, joined by two
// pipes. Each process writes a byte into one pipe and then blocks reading
// from the other. Blocking is what forces the scheduler to switch, so one
// full round trip costs exactly two context switches.
//
// Pinning matters: on a multiprocessor the two processes would otherwise run
// on different cores and simply spin, and we would measure IPC latency
// instead of the cost of stopping one process and resuming another.
//
// The number printed still includes the four pipe system calls of a round
// trip, so it is an upper bound; subtract the syscall cost measured by
// syscall-cost.c for a tighter estimate.
//
// Usage: ./context-switch [iterations] [cpu]   (cpu = -1 disables pinning)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/wait.h>

#define ITERS 100000

static double now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

static void pin_to_cpu(int cpu) {
    if (cpu < 0)                        // -1: let the scheduler place us freely
        return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) < 0) {
        perror("sched_setaffinity");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    int iters = (argc > 1) ? atoi(argv[1]) : ITERS;
    int cpu   = (argc > 2) ? atoi(argv[2]) : 0;

    int p1[2], p2[2];                   // p1: parent -> child, p2: child -> parent
    if (pipe(p1) < 0 || pipe(p2) < 0) {
        perror("pipe");
        exit(1);
    }

    int rc = fork();
    if (rc < 0) {
        perror("fork");
        exit(1);
    }

    char c = 'x';
    if (rc == 0) {
        pin_to_cpu(cpu);
        close(p1[1]);
        close(p2[0]);
        for (int i = 0; i < iters; i++) {
            if (read(p1[0], &c, 1) != 1)    // blocks -> switch to parent
                exit(1);
            if (write(p2[1], &c, 1) != 1)
                exit(1);
        }
        exit(0);
    }

    pin_to_cpu(cpu);
    close(p1[0]);
    close(p2[1]);

    ssize_t r = 0;
    for (int i = 0; i < 1000; i++) {    // warm up
        r += write(p1[1], &c, 1);
        r += read(p2[0], &c, 1);
    }

    double t0 = now_us();
    for (int i = 0; i < iters - 1000; i++) {
        r += write(p1[1], &c, 1);       // wake the child
        r += read(p2[0], &c, 1);        // block -> switch to child
    }
    double t1 = now_us();
    if (r != 2 * iters) {
        fprintf(stderr, "short read/write on the pipe\n");
        exit(1);
    }

    int n = iters - 1000;
    double per_roundtrip = (t1 - t0) * 1000.0 / n;

    if (cpu < 0)
        printf("cpu pinned to           : (not pinned)\n");
    else
        printf("cpu pinned to           : %d\n", cpu);
    printf("round trips             : %d\n", n);
    printf("total time              : %.0f us\n", t1 - t0);
    printf("per round trip          : %.1f ns (= 2 context switches)\n", per_roundtrip);
    printf("per context switch      : %.1f ns\n", per_roundtrip / 2.0);

    close(p1[1]);
    close(p2[0]);
    wait(NULL);
    return 0;
}
