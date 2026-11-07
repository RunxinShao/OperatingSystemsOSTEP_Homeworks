// approx.c -- Q3: the approximate ("sloppy") counter of Fig. 29.4.
//
// usage: ./approx [-t THREADS] [-n INCREMENTS_PER_THREAD] [-S THRESHOLD] [-u]
//   Each thread owns one local counter (the chapter keeps one per CPU; with
//   no more threads than CPUs that is the same thing).  A local counter is
//   moved into the global one whenever it reaches THRESHOLD.
//   -u  pack the local counters next to each other instead of giving each
//       its own 64-byte cache line (shows false sharing).
// Prints one CSV line: layout,threads,per_thread,threshold,seconds,final,expected
#include "common.h"

typedef struct {
    pthread_mutex_t lock;
    long value;
} local_t;                       // 56 bytes on aarch64 glibc

typedef struct {
    local_t l;
    char pad[CACHE_LINE * 2 - sizeof(local_t)];    // one local per 128 bytes
} padded_local_t;

typedef struct {
    long global;
    pthread_mutex_t glock;
    local_t *local[1024];          // local[i] used only by thread i
    int threshold;
} counter_t;

static counter_t c;
static long per_thread = 1000000;
static pthread_barrier_t start;

static void counter_init(counter_t *ctr, int nthreads, int threshold, int packed) {
    ctr->global = 0;
    ctr->threshold = threshold;
    CHECK(pthread_mutex_init(&ctr->glock, NULL));
    // Allocate all locals in one aligned block, either back to back or padded.
    size_t stride = packed ? sizeof(local_t) : sizeof(padded_local_t);
    char *block = aligned_alloc(CACHE_LINE * 2, stride * nthreads + CACHE_LINE * 2);
    for (int i = 0; i < nthreads; i++) {
        ctr->local[i] = (local_t *)(block + i * stride);
        ctr->local[i]->value = 0;
        CHECK(pthread_mutex_init(&ctr->local[i]->lock, NULL));
    }
}

// Fig. 29.4's update(): grab the local lock, bump the local count, and every
// THRESHOLD updates transfer it to the global count under the global lock.
static void update(counter_t *ctr, int tid, long amt) {
    local_t *l = ctr->local[tid];
    pthread_mutex_lock(&l->lock);
    l->value += amt;
    if (l->value >= ctr->threshold) {
        pthread_mutex_lock(&ctr->glock);
        ctr->global += l->value;
        pthread_mutex_unlock(&ctr->glock);
        l->value = 0;
    }
    pthread_mutex_unlock(&l->lock);
}

// get(): only approximate -- leftovers in local counters are not included.
static long get(counter_t *ctr) {
    pthread_mutex_lock(&ctr->glock);
    long v = ctr->global;
    pthread_mutex_unlock(&ctr->glock);
    return v;
}

static void *worker(void *arg) {
    int tid = (int)(intptr_t)arg;
    pthread_barrier_wait(&start);
    for (long i = 0; i < per_thread; i++)
        update(&c, tid, 1);
    return NULL;
}

int main(int argc, char *argv[]) {
    int nthreads = 1, threshold = 1024, packed = 0, opt;
    while ((opt = getopt(argc, argv, "t:n:S:u")) != -1) {
        switch (opt) {
        case 't': nthreads = atoi(optarg); break;
        case 'n': per_thread = atol(optarg); break;
        case 'S': threshold = atoi(optarg); break;
        case 'u': packed = 1; break;
        default:  fprintf(stderr, "usage: %s [-t T] [-n N] [-S THRESHOLD] [-u]\n",
                          argv[0]); return 1;
        }
    }
    assert(nthreads >= 1 && nthreads <= 1024);
    counter_init(&c, nthreads, threshold, packed);
    CHECK(pthread_barrier_init(&start, NULL, nthreads + 1));

    pthread_t tid[1024];
    for (int i = 0; i < nthreads; i++)
        CHECK(pthread_create(&tid[i], NULL, worker, (void *)(intptr_t)i));
    pthread_barrier_wait(&start);
    double t0 = now_sec();
    for (int i = 0; i < nthreads; i++)
        CHECK(pthread_join(tid[i], NULL));
    double secs = now_sec() - t0;

    printf("%s,%d,%ld,%d,%.6f,%ld,%ld\n", packed ? "packed" : "padded", nthreads,
           per_thread, threshold, secs, get(&c), per_thread * nthreads);
    return 0;
}
