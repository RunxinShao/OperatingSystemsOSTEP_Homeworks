// Q5: a reader-writer lock in which neither readers nor writers starve.
//
// Add a "turnstile" in front of the lock (Downey, "The Little Book of
// Semaphores", 4.2.5).  Every reader passes through it on the way in; a
// writer takes it and *keeps* it until it has written.  So once a writer
// is waiting, readers that arrive later queue behind it at the turnstile
// instead of joining the readers already inside.  When those readers
// leave, the writer gets in; when it leaves, the queued readers go.
//
// The turnstile must itself be fair.  With a plain semaphore, a writer that
// releases it can take it straight back before the woken readers run, and
// two writers can then pass it back and forth while every reader starves
// (I measured exactly that).  So the turnstile is the starvation-free mutex
// from Q6 (ns-mutex.h).
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../common_threads.h"
#include "rw-common.h"
// Build with -DPLAIN_TURNSTILE (make reader-writer-plain-turnstile) to use a
// plain semaphore as the turnstile instead, and see readers starve.
#ifndef PLAIN_TURNSTILE
#include "ns-mutex.h"
#else
#define ns_mutex_t           sem_t
#define ns_mutex_init(m)     Sem_init(m, 1)
#define ns_mutex_acquire(m)  Sem_wait(m)
#define ns_mutex_release(m)  Sem_post(m)
#endif

//
// Your code goes in the structure and functions below
//

typedef struct __rwlock_t {
    ns_mutex_t turnstile; // a writer holds this while waiting and writing
    sem_t lock;           // protects readers
    sem_t writelock;      // held by one writer, or by the readers as a group
    int readers;          // readers inside the critical section
} rwlock_t;


void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    ns_mutex_init(&rw->turnstile);
    Sem_init(&rw->lock, 1);
    Sem_init(&rw->writelock, 1);
    hooks_init();
}

void rwlock_acquire_readlock(rwlock_t *rw) {
    double t0 = now_ms();
    ns_mutex_acquire(&rw->turnstile);  // blocked here if a writer is waiting
    ns_mutex_release(&rw->turnstile);
    Sem_wait(&rw->lock);
    rw->readers++;
    if (rw->readers == 1)          // first reader in locks out writers
        Sem_wait(&rw->writelock);
    Sem_post(&rw->lock);
    record_wait(&max_reader_wait_ms, now_ms() - t0);
    usleep(env_us("READ_US"));     // test hook: a long read
}

void rwlock_release_readlock(rwlock_t *rw) {
    Sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0)          // last reader out lets writers in
        Sem_post(&rw->writelock);
    Sem_post(&rw->lock);
}

void rwlock_acquire_writelock(rwlock_t *rw) {
    double t0 = now_ms();
    ns_mutex_acquire(&rw->turnstile);  // stop new readers from getting in
    Sem_wait(&rw->writelock);      // wait for the readers inside to leave
    record_wait(&max_writer_wait_ms, now_ms() - t0);
    usleep(env_us("WRITE_US"));    // test hook: a long write
}

void rwlock_release_writelock(rwlock_t *rw) {
    ns_mutex_release(&rw->turnstile);
    Sem_post(&rw->writelock);
}

//
// Don't change the code below (just use it!)
//

int loops;
int value = 0;

rwlock_t lock;

void *reader(void *arg) {
    int i;
    for (i = 0; i < loops; i++) {
	rwlock_acquire_readlock(&lock);
	printf("read %d\n", value);
	rwlock_release_readlock(&lock);
    }
    return NULL;
}

void *writer(void *arg) {
    int i;
    for (i = 0; i < loops; i++) {
	rwlock_acquire_writelock(&lock);
	value++;
	printf("write %d\n", value);
	rwlock_release_writelock(&lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    assert(argc == 4);
    int num_readers = atoi(argv[1]);
    int num_writers = atoi(argv[2]);
    loops = atoi(argv[3]);

    pthread_t pr[num_readers], pw[num_writers];

    rwlock_init(&lock);

    printf("begin\n");

    int i;
    for (i = 0; i < num_readers; i++)
	Pthread_create(&pr[i], NULL, reader, NULL);
    for (i = 0; i < num_writers; i++)
	Pthread_create(&pw[i], NULL, writer, NULL);

    for (i = 0; i < num_readers; i++)
	Pthread_join(pr[i], NULL);
    for (i = 0; i < num_writers; i++)
	Pthread_join(pw[i], NULL);

    printf("end: value %d\n", value);

    return 0;
}
