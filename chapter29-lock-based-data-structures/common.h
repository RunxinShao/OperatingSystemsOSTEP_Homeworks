// common.h -- helpers shared by the Chapter 29 programs.
#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CACHE_LINE 64

// Wall-clock seconds from a monotonic clock (41 ns resolution on this VM;
// see timer.c / Q1).
static inline double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// Abort on any pthread error, like the book's Pthread_* wrappers.
#define CHECK(x) do { int rc_ = (x); if (rc_ != 0) { \
    fprintf(stderr, "%s:%d: %s failed: %s\n", __FILE__, __LINE__, #x, \
            strerror(rc_)); exit(1); } } while (0)

// Small, fast per-thread PRNG (xorshift64*), so threads never share
// rand()'s hidden lock.
static inline uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

// Start all workers at the same instant: workers and main() each call
// barrier wait, then main() starts the clock.
typedef struct {
    pthread_barrier_t start;
    int nthreads;
} harness_t;

// Run fn(i) on nthreads threads; returns elapsed seconds from the moment
// all threads are released until the last one finishes.
typedef void *(*worker_fn)(void *);
static inline double run_threads(int nthreads, worker_fn fn, void *args, size_t argsz,
                          pthread_barrier_t *start) {
    pthread_t *tid = malloc(sizeof(pthread_t) * nthreads);
    for (int i = 0; i < nthreads; i++)
        CHECK(pthread_create(&tid[i], NULL, fn, (char *)args + i * argsz));
    pthread_barrier_wait(start);
    double t0 = now_sec();
    for (int i = 0; i < nthreads; i++)
        CHECK(pthread_join(tid[i], NULL));
    double t1 = now_sec();
    free(tid);
    return t1 - t0;
}

#endif
