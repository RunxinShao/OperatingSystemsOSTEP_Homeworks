// timer.c -- Q1: how precise are the timers we use for every measurement?
//
// For each clock, call it back to back many times and record
//   - the smallest non-zero difference between two readings (resolution)
//   - the fraction of back-to-back pairs that saw no change
//   - the average cost of one call
#include "common.h"
#include <sys/time.h>

#define N 2000000

// Every clock returns integer nanoseconds.  (Converting seconds-since-1970
// to a double in ns would lose precision: a double near 1.7e18 can only
// represent multiples of 256 ns.)
typedef int64_t ns_t;
static ns_t gtod(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (ns_t)tv.tv_sec * 1000000000 + (ns_t)tv.tv_usec * 1000;
}
static ns_t mono(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ns_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

// The hardware cycle counter: rdtsc on x86, the generic timer's virtual
// count register (cntvct_el0) on ARM.  Returned in ns.
#if defined(__aarch64__)
static double freq;
static ns_t cycles(void) {           // ticks are 41.7 ns, so keep fractions
    uint64_t v;                      // by reporting in units of 0.1 ns
    __asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(v));
    return (ns_t)(v * (1e10 / freq));
}
static void cycles_init(void) {
    uint64_t f;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    freq = (double)f;
    printf("counter: cntvct_el0, frequency %.0f Hz (tick = %.1f ns)\n",
           freq, 1e9 / freq);
}
#elif defined(__x86_64__)
#include <x86intrin.h>
static double freq;
static ns_t cycles(void) { return (ns_t)(__rdtsc() * (1e10 / freq)); }
static void cycles_init(void) {                  // calibrate against mono()
    double t0 = mono(); uint64_t c0 = __rdtsc();
    usleep(200000);
    freq = (__rdtsc() - c0) / ((mono() - t0) / 1e9);
    printf("counter: rdtsc, measured frequency %.0f Hz\n", freq);
}
#endif

// scale: 1 for clocks in ns, 10 for the cycle counter (0.1 ns units)
static void measure(const char *name, ns_t (*clk)(void), double scale) {
    ns_t min = INT64_MAX;
    long zero = 0;
    ns_t prev = clk();
    ns_t start = mono();
    for (int i = 0; i < N; i++) {
        ns_t t = clk();
        ns_t d = t - prev;
        if (d == 0) zero++;
        else if (d > 0 && d < min) min = d;
        prev = t;
    }
    double cost = (double)(mono() - start) / N;
    printf("%-26s smallest step %8.1f ns   unchanged %5.1f%%   cost/call %5.1f ns\n",
           name, min / scale, 100.0 * zero / N, cost);
}

int main(void) {
    cycles_init();
    measure("gettimeofday()", gtod, 1);
    measure("clock_gettime(MONOTONIC)", mono, 1);
    measure("cycle counter", cycles, 10);

    // Sanity check: does a 10 ms sleep measure as about 10 ms on all three?
    ns_t a = gtod(), b = mono(), c = cycles();
    usleep(10000);
    printf("usleep(10000) measured as: gettimeofday %.3f ms, "
           "monotonic %.3f ms, counter %.3f ms\n",
           (gtod() - a) / 1e6, (mono() - b) / 1e6, (cycles() - c) / 1e7);
    return 0;
}
