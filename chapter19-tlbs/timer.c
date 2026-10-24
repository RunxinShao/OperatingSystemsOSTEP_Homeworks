// Q1: how precise are gettimeofday() and clock_gettime()?
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#define N 1000000

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

int main(void) {
    // gettimeofday: smallest observable non-zero step
    struct timeval a, b;
    long min_us = -1, zero = 0;
    for (int i = 0; i < N; i++) {
        gettimeofday(&a, NULL);
        gettimeofday(&b, NULL);
        long d = (b.tv_sec - a.tv_sec) * 1000000L + (b.tv_usec - a.tv_usec);
        if (d == 0) zero++;
        else if (min_us < 0 || d < min_us) min_us = d;
    }
    printf("gettimeofday(): reported unit 1 us, smallest non-zero step %ld us, "
           "%.1f%% of back-to-back pairs saw 0\n", min_us, 100.0 * zero / N);

    // clock_gettime: reported resolution and smallest observable step
    struct timespec res;
    clock_getres(CLOCK_MONOTONIC, &res);
    double min_ns = 1e18;
    for (int i = 0; i < N; i++) {
        double x = now_ns(), y = now_ns();
        if (y > x && y - x < min_ns) min_ns = y - x;
    }
    printf("clock_gettime(MONOTONIC): clock_getres %ld ns, smallest non-zero step %.0f ns\n",
           res.tv_nsec, min_ns);

    // cost of one call
    double t0 = now_ns();
    for (int i = 0; i < N; i++) gettimeofday(&a, NULL);
    double t1 = now_ns();
    for (int i = 0; i < N; i++) now_ns();
    double t2 = now_ns();
    printf("cost per call: gettimeofday %.1f ns, clock_gettime %.1f ns\n",
           (t1 - t0) / N, (t2 - t1) / N);
    return 0;
}
