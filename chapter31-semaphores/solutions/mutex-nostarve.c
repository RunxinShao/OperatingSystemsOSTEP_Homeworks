// Q6: a starvation-free mutex from semaphores, and a test that shows it.
//
// usage: ./mutex-nostarve [THREADS] [SECONDS] [ns|sem]
//   ns   the no-starve mutex (Morris's algorithm, ns-mutex.h)
//   sem  a plain semaphore initialised to 1 used as a lock, for comparison
//
// Each worker loops: acquire, increment a shared counter, release, for
// SECONDS seconds.  To show (no) starvation the program reports, per thread,
//   - how many times it got the lock;
//   - its worst "overtaken" count: how many acquisitions by *other* threads
//     happened between the moment it started waiting and the moment it got
//     the lock.  For Morris's algorithm this is at most 2 x (THREADS - 1)
//     (see README); a plain semaphore gives no bound at all.
//   For "ns" the waiting starts when the thread enters room 1 (recorded by
//   the hook in ns-mutex.h); for "sem" it starts just before sem_wait.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "../common_threads.h"
#include "ns-mutex.h"

static int use_ns = 1;
static ns_mutex_t nsm;
static sem_t plain;

static void lock(void)   { if (use_ns) ns_mutex_acquire(&nsm); else Sem_wait(&plain); }
static void unlock(void) { if (use_ns) ns_mutex_release(&nsm); else Sem_post(&plain); }

static long counter = 0;          // protected by the lock
static volatile int stop = 0;

typedef struct {
    int id;
    long acquired;
    long worst_overtaken;
} worker_t;

void *worker(void *arg) {
    worker_t *w = arg;
    while (!stop) {
        long before = __atomic_load_n(&counter, __ATOMIC_RELAXED);
        lock();
        if (use_ns) before = ns_snapshot;      // taken on entry to room 1
        long overtaken = counter - before;     // others who got in first
        if (overtaken > w->worst_overtaken) w->worst_overtaken = overtaken;
        // the critical section (atomic store only so the hook's read is not
        // a data race; the lock is what makes the increment correct)
        __atomic_store_n(&counter, counter + 1, __ATOMIC_RELAXED);
        w->acquired++;
        unlock();
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int n = argc > 1 ? atoi(argv[1]) : 4;
    double secs = argc > 2 ? atof(argv[2]) : 1.0;
    if (argc > 3) use_ns = strcmp(argv[3], "sem") != 0;

    printf("parent: begin (%d threads, %.1f s, %s)\n", n, secs,
           use_ns ? "no-starve mutex" : "plain semaphore");
    ns_mutex_init(&nsm);
    ns_watch = &counter;
    Sem_init(&plain, 1);

    pthread_t p[n];
    worker_t w[n];
    memset(w, 0, sizeof(w));
    for (int i = 0; i < n; i++) {
        w[i].id = i;
        Pthread_create(&p[i], NULL, worker, &w[i]);
    }
    usleep((useconds_t)(secs * 1e6));
    stop = 1;
    long total = 0, minacq = -1, maxacq = 0, worst = 0;
    for (int i = 0; i < n; i++) {
        Pthread_join(p[i], NULL);
        total += w[i].acquired;
        if (minacq < 0 || w[i].acquired < minacq) minacq = w[i].acquired;
        if (w[i].acquired > maxacq) maxacq = w[i].acquired;
        if (w[i].worst_overtaken > worst) worst = w[i].worst_overtaken;
        printf("thread %2d: acquired %8ld times, worst overtaken %6ld\n",
               i, w[i].acquired, w[i].worst_overtaken);
    }
    printf("counter %ld (sum of acquisitions %ld: %s)\n", counter, total,
           counter == total ? "ok" : "MUTUAL EXCLUSION BROKEN");
    printf("summary: min %ld, max %ld acquisitions per thread, "
           "worst overtaken %ld\n", minacq, maxacq, worst);
    printf("parent: end\n");
    return 0;
}
