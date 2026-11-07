// counter.c -- Q2: a concurrent counter protected by one lock (Fig. 29.2).
//
// usage: ./counter [-t THREADS] [-n INCREMENTS_PER_THREAD] [-m MODE]
//   MODE  mutex   pthread_mutex_t around every increment (the chapter's version)
//         spin    test-and-set spin lock from Chapter 28
//         atomic  one hardware atomic add per increment, no lock
//         none    no synchronization at all (wrong on purpose: Chapter 26)
// Prints one CSV line: mode,threads,per_thread,seconds,final,expected
#include "common.h"

typedef struct {
    volatile long value;           // volatile so "none" really races in memory
    pthread_mutex_t lock;
    volatile int spin;             // 0 = free, 1 = held
} counter_t;

static counter_t c;
static int mode;                   // 0 mutex, 1 spin, 2 atomic, 3 none
static long per_thread = 1000000;
static pthread_barrier_t start;

static void spin_lock(volatile int *l) {
    while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE) == 1)
        ;                          // TestAndSet, as in Fig. 28.3
}
static void spin_unlock(volatile int *l) { __atomic_store_n(l, 0, __ATOMIC_RELEASE); }

static void increment(counter_t *ctr) {
    switch (mode) {
    case 0: pthread_mutex_lock(&ctr->lock); ctr->value++;
            pthread_mutex_unlock(&ctr->lock); break;
    case 1: spin_lock(&ctr->spin); ctr->value++; spin_unlock(&ctr->spin); break;
    case 2: __atomic_fetch_add(&ctr->value, 1, __ATOMIC_RELAXED); break;
    case 3: ctr->value++; break;
    }
}

static void *worker(void *arg) {
    (void)arg;
    pthread_barrier_wait(&start);
    for (long i = 0; i < per_thread; i++)
        increment(&c);
    return NULL;
}

int main(int argc, char *argv[]) {
    int nthreads = 1, opt;
    const char *names[] = {"mutex", "spin", "atomic", "none"};
    while ((opt = getopt(argc, argv, "t:n:m:")) != -1) {
        switch (opt) {
        case 't': nthreads = atoi(optarg); break;
        case 'n': per_thread = atol(optarg); break;
        case 'm': for (mode = 0; mode < 4 && strcmp(optarg, names[mode]); mode++)
                      ;
                  if (mode == 4) { fprintf(stderr, "bad mode\n"); return 1; }
                  break;
        default:  fprintf(stderr, "usage: %s [-t T] [-n N] [-m mutex|spin|atomic|none]\n",
                          argv[0]); return 1;
        }
    }
    CHECK(pthread_mutex_init(&c.lock, NULL));
    CHECK(pthread_barrier_init(&start, NULL, nthreads + 1));
    double secs = run_threads(nthreads, worker, NULL, 0, &start);
    printf("%s,%d,%ld,%.6f,%ld,%ld\n", names[mode], nthreads, per_thread, secs,
           c.value, per_thread * nthreads);
    return 0;
}
