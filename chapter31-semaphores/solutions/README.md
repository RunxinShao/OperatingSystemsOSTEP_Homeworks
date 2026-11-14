# Chapter 31 — Semaphores

This chapter's homework is code: fill in the skeletons in `threads-sema/`
using only semaphores. My solutions are in `threads-sema/solutions/`. They
include the upstream `../common_threads.h`, so `Sem_init`, `Sem_wait` and
`Sem_post` are the usual wrappers around `sem_init`, `sem_wait` and
`sem_post` that abort on any error. I left the "don't change below here"
part of every skeleton unchanged.

| file | question |
|---|---|
| `fork-join.c` | Q1 |
| `rendezvous.c` | Q2 |
| `barrier.c` | Q3 |
| `reader-writer.c` | Q4 (readers can starve writers) |
| `reader-writer-nostarve.c` | Q5 (no starvation; also builds `reader-writer-plain-turnstile`) |
| `ns-mutex.h`, `mutex-nostarve.c` | Q6 (starvation-free mutex and a test for it) |
| `rw-common.h` | test hooks for Q4/Q5: `READ_US`, `WRITE_US`, longest-wait report |
| `test.sh` | runs everything, checks each ordering property, saves `data/*.txt` |

```sh
make            # gcc -Wall -Wextra -O2 -pthread, no warnings
./test.sh       # about 1 minute; prints PASS/FAIL for each property
```

```
PASS  fork-join: parent waits for the sleeping child
PASS  rendezvous: no 'after' before both 'before's (500 runs + sleeps)
PASS  barrier: 1..32 threads x 200 runs
PASS  barrier: late thread holds everyone back
PASS  reader-writer: value = writers x loops (16 threads, no sleeps)
PASS  reader-writer-plain-turnstile: value = writers x loops (16 threads, no sleeps)
PASS  reader-writer-nostarve: value = writers x loops (16 threads, no sleeps)
PASS  mutex-nostarve: worst overtaken <= 2(N-1) for N = 2..16
```

Machine: the same OrbStack VM as before (Linux 7.0.5 aarch64, 18 vCPUs,
glibc). One fact about it matters a lot in Q5 and Q6: **POSIX semaphores
on Linux are not FIFO**. `sem_post` wakes *a* waiter, but if the posting
thread calls `sem_wait` again before the woken thread actually runs, the
poster can take the semaphore straight back. This is called barging.

---

### 1. Fork/join (`fork-join.c`)

```c
sem_t s;

void *child(void *arg) {
    sleep(1);                      // make the child slow on purpose
    printf("child\n");
    Sem_post(&s);                  // signal: child is done
    return NULL;
}
...
    Sem_init(&s, 0);               // 0: nothing has happened yet
    Pthread_create(&p, NULL, child, NULL);
    Sem_wait(&s);                  // wait here for the child
```

```
parent: begin
child
parent: end
```

The semaphore starts at **0**, so it counts events that have *happened*.
There are two cases:

- The parent reaches `Sem_wait` first, which is what the `sleep(1)` forces.
  The value drops to -1 and the parent sleeps until the child posts.
- The child posts first. The value becomes 1, and the parent's `Sem_wait`
  returns immediately.

Either way, "child" is printed before "parent: end". If the semaphore
started at 1, the parent would not wait at all.

---

### 2. Rendezvous (`rendezvous.c`)

Two semaphores, both starting at 0. Each one means "child *i* has
arrived". Each child **announces its own arrival first** and then waits
for the other's:

```c
void *child_1(void *arg) {           void *child_2(void *arg) {
    printf("child 1: before\n");         printf("child 2: before\n");
    Sem_post(&s1);                       Sem_post(&s2);
    Sem_wait(&s2);                       Sem_wait(&s1);
    printf("child 1: after\n");          printf("child 2: after\n");
```

`./rendezvous 1` makes child 1 sleep 1 s before it arrives, and
`./rendezvous 2` does the same for child 2 (`data/rendezvous.txt`):

```
$ ./rendezvous 1                 $ ./rendezvous 2
parent: begin                    parent: begin
child 2: before                  child 1: before
child 1: before                  child 2: before
child 1: after                   child 2: after
child 2: after                   child 1: after
parent: end                      parent: end
```

Whichever child arrives first waits for the other. `test.sh` also runs it
500 times without the sleeps and checks that no "after" line ever comes
before both "before" lines.

The **order** of post and wait matters. If both children did
`wait; post` instead, each would wait for a post that the other never
reaches, and the program would **deadlock**: each child holds back
something the other needs. That is the circular wait of Chapter 32. Doing
`post` first in one child and `wait` first in the other also works, but
it costs an extra context switch.

---

### 3. Barrier (`barrier.c`)

```c
typedef struct __barrier_t {
    sem_t mutex;          // protects count
    sem_t turnstile;      // threads wait here until the last one arrives
    int count;            // how many threads have arrived
    int n;                // how many threads must arrive
} barrier_t;

void barrier(barrier_t *b) {
    Sem_wait(&b->mutex);
    b->count++;
    int last = (b->count == b->n);
    Sem_post(&b->mutex);

    if (last)                      // the last arrival lets everyone through:
        for (int i = 0; i < b->n; i++)
            Sem_post(&b->turnstile);
    Sem_wait(&b->turnstile);
}
```

- `mutex`, a semaphore initialized to 1, used as a lock, protects the arrival
  count.
- `turnstile` starts at 0. Everyone waits on it. The **last** thread to
  arrive posts it `n` times, once for every thread including itself, so all
  `n` waits succeed.
- I deliberately read `count == n` *inside* the mutex and post the turnstile
  *outside* it. That way no thread sleeps on the turnstile while holding the
  mutex.

Test with a late thread: with `LATE=1`, the first thread to arrive sleeps
1 s before entering the barrier (`data/barrier.txt`, timestamps added by
`test.sh`):

```
$ LATE=1 ./barrier 4      (first arrival sleeps 1 s)
 0.00 s  child 0: before
 0.00 s  child 1: before
 0.00 s  child 2: before
 0.00 s  child 3: before
 1.00 s  child 0: after
 1.00 s  child 3: after
 1.00 s  child 1: after
 1.00 s  child 2: after
```

Three threads arrived at once, and none of them got past the barrier until
the late one arrived a second later. `test.sh` also runs 1, 2, 3, 4, 8, 16
and 32 threads 200 times each and checks that every "before" precedes every
"after".

**Limitation:** this barrier works only **once**, which is all the skeleton
uses. To reuse it in a loop, `count` must be reset, and a fast thread must not
be able to run through the barrier a second time while slow threads are still
leaving the first one. The usual fix is a second turnstile, so the barrier
has two phases (Downey 3.7). `pthread_barrier_wait`, which I used in
Chapter 29 to start threads together, is the library's reusable version.

---

### 4. Reader-writer lock, readers first (`reader-writer.c`)

This is the lock from Figure 31.13. The first reader in takes `writelock`
on behalf of all readers, and the last reader out releases it. A writer
simply waits on `writelock`:

```c
void rwlock_acquire_readlock(rwlock_t *rw) {
    Sem_wait(&rw->lock);
    rw->readers++;
    if (rw->readers == 1)          // first reader in locks out writers
        Sem_wait(&rw->writelock);
    Sem_post(&rw->lock);
}
void rwlock_release_readlock(rwlock_t *rw) {
    Sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0)          // last reader out lets writers in
        Sem_post(&rw->writelock);
    Sem_post(&rw->lock);
}
void rwlock_acquire_writelock(rwlock_t *rw) { Sem_wait(&rw->writelock); }
void rwlock_release_writelock(rwlock_t *rw) { Sem_post(&rw->writelock); }
```

**Demonstrating starvation.** The reads in the skeleton are only a `printf`,
so they rarely overlap. I added two test hooks, both inside the lock
functions, so the skeleton's `main` is untouched (`rw-common.h`):

- `READ_US` / `WRITE_US`: hold the lock N µs longer, the "add sleep() calls"
  the question asks for.
- At exit, the longest time any writer or reader waited for the lock is
  printed on stderr.

```
$ READ_US=10000 WRITE_US=10000 ./reader-writer 3 2 5
longest wait: writer 118.3 ms, reader 0.0 ms
begin read 0 read 0 read 0 read 0 read 0 read 0 read 0 read 0 read 0 read 0
read 0 read 0 read 0 read 0 read 0 write 1 write 2 write 3 write 4 write 5
write 6 write 7 write 8 write 9 write 10 end: value 10
```

All 15 reads happen first, and every one of them reads 0, before the first
write. The three readers' 10 ms reads overlap, so at least one reader is
always inside, and `readers` never drops to 0 until the readers have
**finished all their loops**. The writers starve for the readers' entire
lifetime:

| `READ_US=1000 ./reader-writer 8 2 200`, 3 runs | longest writer wait | longest reader wait |
|---|---|---|
| readers-first lock | **577 / 376 / 376 ms** | 0.0 ms |

With readers that never stop arriving, for example a server that keeps
handling read requests, the writer would wait forever. Readers never wait
(0.0 ms), so this lock is maximally unfair to writers.

---

### 5. Reader-writer lock without starvation (`reader-writer-nostarve.c`)

The idea is a **turnstile** in front of the room (Downey 4.2.5):

- Every reader passes through the turnstile on its way in: acquire, then
  release immediately.
- A writer acquires the turnstile and **keeps it** while it waits for the
  room to empty and while it writes.

So once a writer is waiting, readers that arrive later queue at the
turnstile behind it. The readers already inside drain out, the writer goes
in, and when it leaves, the queued readers go next. Readers that are
already inside still share the lock with each other.

```c
void rwlock_acquire_readlock(rwlock_t *rw) {
    ns_mutex_acquire(&rw->turnstile);  // blocked here if a writer is waiting
    ns_mutex_release(&rw->turnstile);
    ... same as Q4 ...
}
void rwlock_acquire_writelock(rwlock_t *rw) {
    ns_mutex_acquire(&rw->turnstile);  // stop new readers from getting in
    Sem_wait(&rw->writelock);          // wait for the readers inside to leave
}
void rwlock_release_writelock(rwlock_t *rw) {
    ns_mutex_release(&rw->turnstile);
    Sem_post(&rw->writelock);
}
```

**My first version used a plain semaphore as the turnstile, and it was
wrong.** It is still in the file and builds as
`reader-writer-plain-turnstile`. It stopped the readers from starving the
writers, but now the **writers starved the readers**:

```
$ READ_US=10000 WRITE_US=10000 ./reader-writer-plain-turnstile 3 2 5
longest wait: writer 71.8 ms, reader 121.1 ms
begin read 0 read 0 read 0 write 1 write 2 write 3 write 4 write 5 write 6
write 7 write 8 write 9 write 10 read 10 read 10 ... end: value 10
```

The cause is barging. A writer releases the turnstile and loops straight
back to `sem_wait`, so it, or the other writer it hands off to, gets the
turnstile again before the readers that were woken ever run. The two
writers passed the turnstile back and forth until they had finished all 10
writes. "Waiting in line at the turnstile" only works if the turnstile is a
**fair** queue.

So the turnstile is the starvation-free mutex from Q6 (`ns-mutex.h`).
With it, reads and writes interleave, and **both** kinds of waits are
bounded:

```
$ READ_US=10000 WRITE_US=10000 ./reader-writer-nostarve 3 2 5
longest wait: writer 48.6 ms, reader 67.6 ms
begin read 0 read 0 read 0 write 1 read 1 read 1 write 2 write 3 read 3
write 4 read 4 write 5 read 5 write 6 write 7 read 7 read 7 write 8 write 9
read 9 write 10 read 10 read 10 read 10 read 10 end: value 10
```

| `READ_US=1000 ./PROG 8 2 200` (3 runs) | longest writer wait | longest reader wait |
|---|---|---|
| Q4 readers-first | 577 / 376 / 376 ms | 0.0 ms |
| plain-semaphore turnstile | 79 / 99 / 119 ms | **255 / 297 / 316 ms** |
| **no-starve** (fair turnstile) | **12 / 8 / 9 ms** | **11 / 10 / 11 ms** |

Every thread now makes progress: the worst wait for either kind is about
10 ms, roughly one round of the other threads. Correctness is unchanged: all
three versions end with `value` = writers x loops, including a 4 readers +
4 writers x 2000 loops run with no sleeps (`test.sh`).

---

### 6. A starvation-free mutex (`ns-mutex.h`, `mutex-nostarve.c`)

A semaphore initialized to 1 is already a mutex, but a **weak** one, as Q5
showed. A thread can lose the race for it any number of times. I
implemented **Morris's algorithm** (Downey 4.3), which is starvation-free
even though it is built from weak semaphores:

```c
typedef struct __ns_mutex_t {
    int room1, room2;     // threads waiting in each room
    sem_t mutex;          // protects room1
    sem_t t1;             // turnstile into room 2 (starts open = 1)
    sem_t t2;             // turnstile into the critical section (starts 0)
} ns_mutex_t;

void ns_mutex_acquire(ns_mutex_t *m) {
    Sem_wait(&m->mutex); m->room1++; Sem_post(&m->mutex);   // enter room 1
    Sem_wait(&m->t1);                                       // to room 2
    m->room2++;
    Sem_wait(&m->mutex);
    m->room1--;
    if (m->room1 == 0) { Sem_post(&m->mutex); Sem_post(&m->t2); } // last: open t2
    else               { Sem_post(&m->mutex); Sem_post(&m->t1); } // next one in
    Sem_wait(&m->t2);                                       // critical section
    m->room2--;
}
void ns_mutex_release(ns_mutex_t *m) {
    if (m->room2 == 0) { Sem_post(&m->t1); }   // batch done: reopen room 1
    else               { Sem_post(&m->t2); }   // next thread of this batch
}
```

**How it works:**

1. Threads gather in **room 1**. They move one at a time through `t1` into
   **room 2**.
2. The thread that empties room 1 closes `t1` (by not posting it) and opens
   `t2`. The threads in room 2 now form a **batch** and run the critical
   section one at a time.
3. Only after the whole batch has gone through does the last one reopen
   `t1`.

A thread that finishes and immediately calls acquire again lands in room 1
and **cannot overtake** anyone already in room 2. A thread that has entered
room 1 waits at most for the current batch, at most N-1 threads, and then
for its own batch, at most N-1 more. So it is overtaken **at most
2(N-1) times**, whatever the semaphores' wake-up order.

**Demonstrating it.** The question asks how to *show* that a thread does not
starve. `mutex-nostarve` runs N threads that do nothing but
`acquire; counter++; release` for 1 second. For every acquisition it records
how many acquisitions *by others* happened between the moment the thread
started waiting (entering room 1) and the moment it got the lock: its
**overtaken** count. The mode `sem` runs the same test with a plain
semaphore as the lock.

| threads | no-starve: worst overtaken | bound 2(N-1) | plain semaphore: worst overtaken |
|---|---|---|---|
| 2 | **2** | 2 | 12,784 |
| 4 | **6** | 6 | 34,252 |
| 8 | **14** | 14 | 18,309 |
| 16 | **30** (27–30 for every thread) | 30 | 2,667 |

```
$ ./mutex-nostarve 8 1 ns
thread  0: acquired    17720 times, worst overtaken     14
...
thread  7: acquired    17411 times, worst overtaken     14
counter 139590 (sum of acquisitions 139590: ok)
```

- The measured worst case for the no-starve mutex **equals the theoretical
  bound exactly** for every N, and `test.sh` checks it. With a plain
  semaphore, a thread waited while others got in up to **34,252 times in a
  row**. Nothing bounds that number. It only stays finite here because the
  test ends after a second.
- `counter` always equals the sum of all acquisitions, so mutual exclusion
  holds in both modes.
- **The price is throughput.** The plain semaphore did 5–7 M acquisitions/s
  with 4–8 threads, and the no-starve mutex did only 0.14–0.23 M/s, about
  30x less. Morris hands the lock directly to a sleeping thread, so nearly
  every acquisition costs a futex wake-up and a context switch. The
  barging semaphore lets the thread that is already running keep the lock,
  with no context switch. This is the same trade-off as the ticket lock in
  Chapter 28 (Q11), where fairness forced handoffs to threads that were not
  running.
- **Starvation-free is not the same as equal shares.** In one 4-thread run,
  one thread got 136,886 acquisitions and the others about 31,000 each. A
  thread that is not waiting (here, one whose vCPU was slow to come back)
  is not overtaken, it is just absent. The bound covers how long a waiting
  thread can be passed over, and that stayed at 6.

---

### 7. More problems

Downey's *The Little Book of Semaphores* is where the turnstile (Q5),
the two-phase reusable barrier (Q3) and Morris's algorithm (Q6) come from.

---

## Connections to earlier chapters

- **Locks (Ch. 28).** A binary semaphore (initial value 1) is a lock, and
  `sem_wait`/`sem_post` on Linux are built the same way as the futex-based
  two-phase lock at the end of Chapter 28. The Q6 measurements are
  Chapter 28's fairness discussion on real hardware. A plain semaphore is
  like test-and-set: whoever is running wins, with no bound on waiting. The
  no-starve mutex is like the ticket lock: bounded waiting, at the cost of
  forced handoffs and much lower throughput.
- **Condition variables (Ch. 30).** Q1 is the `thr_join` problem from
  Chapter 30. The semaphore needs no `done` flag because its *value* is the
  state: a post that happens before the wait is remembered, whereas a
  `cond_signal` with no waiter is lost. Q3's barrier and Q4's
  reader-writer lock could equally be written with a mutex plus condition
  variables. Chapter 31 shows that a semaphore can stand in for both.
- **Scheduling (Ch. 7–9).** Starvation in Q4–Q6 is the same problem MLFQ
  solves with priority boost (Ch. 8): a policy that always favours one class
  (readers in Q4, the running thread in Q6's plain semaphore) can wait
  forever before serving the other. Morris's batches work like round-robin:
  every waiting thread gets one turn before anyone gets a second. That
  removes starvation at the cost of more switches, just as a short
  round-robin quantum trades throughput for response time.
- **Context switches (Ch. 6).** The throughput gap in Q6 (about 30x) is
  the cost of a futex sleep, a wake-up and a context switch per handoff.
  This is the same effect seen in Chapter 29, where the B-tree rwlock with
  2 threads made 75,875 context switches and slowed down 8x.
- **Lock-based data structures (Ch. 29).** I used `pthread_rwlock` in the
  Chapter 29 B-tree. Q4 shows how such a lock can be built and why its
  policy matters. My Chapter 29 measurements found the glibc rwlock
  collapsing when writers mixed with readers, and the starvation/fairness
  trade-off here is the reason libraries make that choice carefully.
- **Deadlock (Ch. 32).** The rendezvous only works with post-before-wait.
  The opposite order is a circular wait in which both threads hold back
  the signal the other needs. The barrier releases its `mutex` before
  sleeping on the turnstile. Sleeping while holding a lock that the waker
  needs is the classic semaphore deadlock (Chapter 31's bounded-buffer
  example, where the mutex is taken before `empty`/`full`).
