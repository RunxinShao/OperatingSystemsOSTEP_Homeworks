// Q3: a barrier for N threads built from two semaphores and a counter.
//
// usage: ./barrier NUM_THREADS
//   With LATE=1 in the environment, the first thread to reach the barrier
//   sleeps 1 second before entering it, so everyone else must wait for it.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../common_threads.h"

typedef struct __barrier_t {
    sem_t mutex;          // protects count
    sem_t turnstile;      // threads wait here until the last one arrives
    int count;            // how many threads have arrived
    int n;                // how many threads must arrive
} barrier_t;

// the single barrier we are using for this program
barrier_t b;

void barrier_init(barrier_t *b, int num_threads) {
    Sem_init(&b->mutex, 1);
    Sem_init(&b->turnstile, 0);
    b->count = 0;
    b->n = num_threads;
}

static int late_arrivals = 0;     // test hook only (see LATE above)

void barrier(barrier_t *b) {
    if (getenv("LATE") && __atomic_fetch_add(&late_arrivals, 1, __ATOMIC_SEQ_CST) == 0)
        sleep(1);

    Sem_wait(&b->mutex);
    b->count++;
    int last = (b->count == b->n);
    Sem_post(&b->mutex);

    if (last)                      // the last arrival lets everyone through:
        for (int i = 0; i < b->n; i++)     // n posts, one per thread
            Sem_post(&b->turnstile);       // (including itself)
    Sem_wait(&b->turnstile);
}

//
// XXX: don't change below here (just run it!)
//
typedef struct __tinfo_t {
    int thread_id;
} tinfo_t;

void *child(void *arg) {
    tinfo_t *t = (tinfo_t *) arg;
    printf("child %d: before\n", t->thread_id);
    barrier(&b);
    printf("child %d: after\n", t->thread_id);
    return NULL;
}


// run with a single argument indicating the number of
// threads you wish to create (1 or more)
int main(int argc, char *argv[]) {
    assert(argc == 2);
    int num_threads = atoi(argv[1]);
    assert(num_threads > 0);

    pthread_t p[num_threads];
    tinfo_t t[num_threads];

    printf("parent: begin\n");
    barrier_init(&b, num_threads);

    int i;
    for (i = 0; i < num_threads; i++) {
	t[i].thread_id = i;
	Pthread_create(&p[i], NULL, child, &t[i]);
    }

    for (i = 0; i < num_threads; i++)
	Pthread_join(p[i], NULL);

    printf("parent: end\n");
    return 0;
}
