// Chapter 6, measurement homework -- part 1: how good is the timer?
//
// gettimeofday() reports microseconds since 1970, but reporting microseconds
// is not the same as resolving microseconds. We measure two things:
//   (a) resolution -- the smallest non-zero delta two calls can ever show
//   (b) cost       -- how long one call actually takes
// The cost is what sets the floor on how many iterations any later timing
// loop needs: to keep timer error under 1%, the loop must last >= 100x the
// cost of a single gettimeofday() call.

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>

#define ITERS 1000000

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t) hi << 32) | lo;
}

int main(void) {
    struct timeval a, b;

    // (a) resolution: the smallest non-zero difference we ever observe
    long min_delta = 1000000;
    int zero_deltas = 0;
    for (int i = 0; i < ITERS; i++) {
        gettimeofday(&a, NULL);
        gettimeofday(&b, NULL);
        long d = (b.tv_sec - a.tv_sec) * 1000000L + (b.tv_usec - a.tv_usec);
        if (d == 0)
            zero_deltas++;
        else if (d < min_delta)
            min_delta = d;
    }
    printf("back-to-back gettimeofday():\n");
    printf("  smallest non-zero delta : %ld us\n", min_delta);
    printf("  calls that saw delta 0  : %d / %d (%.1f%%)\n",
           zero_deltas, ITERS, 100.0 * zero_deltas / ITERS);

    // (b) cost of one call, amortised over a long loop
    gettimeofday(&a, NULL);
    for (int i = 0; i < ITERS; i++)
        gettimeofday(&b, NULL);
    struct timeval end;
    gettimeofday(&end, NULL);
    double total = (end.tv_sec - a.tv_sec) * 1e6 + (end.tv_usec - a.tv_usec);
    printf("  cost of one call        : %.1f ns\n", total * 1000.0 / ITERS);

    // (c) rdtsc, for comparison: a single instruction, no kernel entry
    uint64_t t0 = rdtsc();
    for (int i = 0; i < ITERS; i++)
        (void) rdtsc();
    uint64_t t1 = rdtsc();
    printf("  cost of one rdtsc       : %.1f cycles\n", (double) (t1 - t0) / ITERS);
    return 0;
}
