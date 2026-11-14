// ns-mutex.h -- Q6: a starvation-free mutex built only from semaphores.
//
// This is Morris's algorithm (J. M. Morris, 1979; Downey, "The Little Book
// of Semaphores", 4.3).  It works even though POSIX semaphores are *weak*:
// sem_post wakes some waiter, but a thread that calls sem_wait right after
// can barge in first, so a plain semaphore used as a lock can starve a
// thread forever.
//
// Threads pass through two waiting rooms.  Arrivals gather in room 1.  The
// turnstile t1 lets them into room 2 one at a time; when room 1 is empty,
// the last one in opens t2, and the threads in room 2 then take the
// critical section one at a time.  t1 stays closed until room 2 is empty,
// so a thread that finishes and comes back must wait in room 1 behind
// everyone already in room 2.  Every waiter is therefore overtaken at most
// a bounded number of times: no starvation.
#ifndef NS_MUTEX_H
#define NS_MUTEX_H
#include "../common_threads.h"

typedef struct __ns_mutex_t {
    int room1, room2;     // threads waiting in each room
    sem_t mutex;          // protects room1
    sem_t t1;             // turnstile into room 2 (open = 1)
    sem_t t2;             // turnstile into the critical section
} ns_mutex_t;

// Measurement hook for mutex-nostarve.c: if ns_watch is set, each thread
// records *ns_watch at the moment it enters room 1, i.e. the moment it
// really starts waiting.
static long *ns_watch = 0;
static __thread long ns_snapshot;

static void ns_mutex_init(ns_mutex_t *m) {
    m->room1 = m->room2 = 0;
    Sem_init(&m->mutex, 1);
    Sem_init(&m->t1, 1);
    Sem_init(&m->t2, 0);
}

static void ns_mutex_acquire(ns_mutex_t *m) {
    Sem_wait(&m->mutex);           // enter room 1
    m->room1++;
    if (ns_watch) ns_snapshot = __atomic_load_n(ns_watch, __ATOMIC_RELAXED);
    Sem_post(&m->mutex);

    Sem_wait(&m->t1);              // move to room 2 (one at a time)
    m->room2++;                    // (protected by holding t1)
    Sem_wait(&m->mutex);
    m->room1--;
    if (m->room1 == 0) {           // room 1 empty: close t1, open t2
        Sem_post(&m->mutex);
        Sem_post(&m->t2);
    } else {                       // more in room 1: let the next one through
        Sem_post(&m->mutex);
        Sem_post(&m->t1);
    }

    Sem_wait(&m->t2);              // enter the critical section
    m->room2--;                    // (protected by holding t2)
}

static void ns_mutex_release(ns_mutex_t *m) {
    // (braces needed: the Sem_* macros end in their own semicolon)
    if (m->room2 == 0) {           // room 2 empty: reopen room 1's turnstile
        Sem_post(&m->t1);
    } else {                       // otherwise the next thread in room 2
        Sem_post(&m->t2);
    }
}
#endif
