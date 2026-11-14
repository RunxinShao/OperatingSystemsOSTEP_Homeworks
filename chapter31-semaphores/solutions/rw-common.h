// rw-common.h -- test hooks shared by reader-writer.c and
// reader-writer-nostarve.c (Q4, Q5).
//
//   READ_US=N   a reader keeps the read lock for N extra microseconds, so
//               the reads of several readers overlap
//   WRITE_US=N  same for a writer
// The longest time any writer waited for the write lock is printed on
// stderr at exit, so starvation can be measured and not just eyeballed.
#ifndef RW_COMMON_H
#define RW_COMMON_H
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}
static int env_us(const char *name) {
    const char *v = getenv(name);
    return v ? atoi(v) : 0;
}

static double max_writer_wait_ms = 0, max_reader_wait_ms = 0;
static sem_t stats_lock;
static void record_wait(double *max, double waited) {
    Sem_wait(&stats_lock);
    if (waited > *max) *max = waited;
    Sem_post(&stats_lock);
}
static void print_waits(void) {
    fprintf(stderr, "longest wait: writer %.1f ms, reader %.1f ms\n",
            max_writer_wait_ms, max_reader_wait_ms);
}
static void hooks_init(void) {
    Sem_init(&stats_lock, 1);
    atexit(print_waits);
}
#endif
