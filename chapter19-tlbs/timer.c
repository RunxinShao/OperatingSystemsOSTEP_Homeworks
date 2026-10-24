// Q1: how precise are gettimeofday() and clock_gettime()?
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#define N 1000000

static double now_ns(clockid_t clk) {
    struct timespec ts;
    clock_gettime(clk, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

// smallest non-zero step seen between back-to-back reads of a clock
static void probe(const char *name, clockid_t clk) {
    struct timespec res;
    clock_getres(clk, &res);
    double min_ns = 1e18;
    for (int i = 0; i < N; i++) {
        double x = now_ns(clk), y = now_ns(clk);
        if (y > x && y - x < min_ns) min_ns = y - x;
    }
    double t0 = now_ns(clk);
    for (int i = 0; i < N; i++) now_ns(clk);
    double t1 = now_ns(clk);
    printf("clock_gettime(%s): clock_getres %ld ns, smallest non-zero step %.0f ns, "
           "cost %.1f ns\n", name, res.tv_nsec, min_ns, (t1 - t0) / N);
}

int main(void) {
    struct timeval a, b;
    long min_us = -1, zero = 0;
    for (int i = 0; i < N; i++) {
        gettimeofday(&a, NULL);
        gettimeofday(&b, NULL);
        long d = (b.tv_sec - a.tv_sec) * 1000000L + (b.tv_usec - a.tv_usec);
        if (d == 0) zero++;
        else if (min_us < 0 || d < min_us) min_us = d;
    }
    double t0 = now_ns(CLOCK_MONOTONIC_RAW);
    for (int i = 0; i < N; i++) gettimeofday(&a, NULL);
    double t1 = now_ns(CLOCK_MONOTONIC_RAW);
    printf("gettimeofday(): reported unit 1 us, smallest non-zero step %ld us, "
           "%.1f%% of back-to-back pairs saw 0, cost %.1f ns\n",
           min_us, 100.0 * zero / N, (t1 - t0) / N);

    probe("MONOTONIC", CLOCK_MONOTONIC);
    probe("MONOTONIC_RAW", CLOCK_MONOTONIC_RAW);   // what tlb.c uses
    return 0;
}
