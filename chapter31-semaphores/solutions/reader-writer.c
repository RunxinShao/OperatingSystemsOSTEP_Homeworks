// Q4: reader-writer lock from semaphores (Figure 31.13).  Readers have
// priority: as long as at least one reader holds the lock, new readers get
// in, so a steady stream of readers starves the writers.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../common_threads.h"
#include "rw-common.h"

//
// Your code goes in the structure and functions below
//

typedef struct __rwlock_t {
    sem_t lock;           // protects readers
    sem_t writelock;      // held by one writer, or by the readers as a group
    int readers;          // readers inside the critical section
} rwlock_t;


void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    Sem_init(&rw->lock, 1);
    Sem_init(&rw->writelock, 1);
    hooks_init();
}

void rwlock_acquire_readlock(rwlock_t *rw) {
    double t0 = now_ms();
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
    Sem_wait(&rw->writelock);
    record_wait(&max_writer_wait_ms, now_ms() - t0);
    usleep(env_us("WRITE_US"));    // test hook: a long write
}

void rwlock_release_writelock(rwlock_t *rw) {
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
