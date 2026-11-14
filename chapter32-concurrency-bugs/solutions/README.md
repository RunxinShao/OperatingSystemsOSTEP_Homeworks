# Chapter 32 — Common Concurrency Problems

Machine: aarch64 Linux VM (OrbStack, 18 vCPUs) on an Apple M5 Max host
(6 performance + 12 efficiency cores), gcc 15.2.0. The host is shared, so
every timing below is the **median of 5 runs** of the program's own `-t`
output (which only has 10 ms resolution), and every "hung" count is the number
of runs that `timeout` had to kill.

Build and run from `threads-bugs/solutions/`:

```
make            # builds the 5 variants + helper programs into solutions/
./run.sh all    # every experiment below; raw output goes to data/*.txt
./run.sh q7     # or a single section: q1 q2 q3 q4 q5 q6 q7 q8 q9 q10 knee final
```

`solutions/Makefile` compiles the four lock-based variants straight from the
upstream sources (`../vector-*.c`, with `-I..`) and uses the same flags as the
upstream Makefile (`-Wall -pthread -O`). Nothing in `threads-bugs/` itself is
modified.

**aarch64 port of vector-nolock.c.** The upstream `fetch_and_add()` is x86
inline assembly (`lock; xaddl`), so on this machine gcc stops with
`error: impossible constraint in 'asm'`. `solutions/vector-nolock.c` is a copy
that keeps the x86 asm under `#if defined(__x86_64__)` and otherwise uses

```
return __atomic_fetch_add(variable, value, __ATOMIC_SEQ_CST);
```

which has the same meaning: atomically add and return the old value. gcc
calls its outline-atomics helper. The helper checks once whether the CPU has
the ARMv8.1 LSE atomics (`/proc/cpuinfo` lists `atomics` here) and then uses
a single `ldaddal` (LSE `ldadd` with acquire+release). Otherwise it falls back
to an `ldaxr`/`stlxr` retry loop (`objdump -d vector-nolock`):

```
00000000000014d0 <__aarch64_ldadd4_acq_rel>:
    14d8:  cbz     w16, 14e4         <- no LSE? use the LL/SC loop
    14dc:  ldaddal w0, w0, [x1]      <- one atomic fetch-and-add
    14e0:  ret
    14e8:  ldaxr   w0, [x1]          <- fallback: load-exclusive
    14ec:  add     w17, w0, w16
    14f0:  stlxr   w15, w17, [x1]    <- store-exclusive, retry if it failed
```

(With `-march=armv8.1-a` gcc puts `ldaddal w1, w0, [x0]` straight into
`fetch_and_add`.)

Helper programs (also built by `make`):

- `self-add.c` calls `vector_add(&v[0], &v[0])` with a chosen variant
  (question 4).
- `check-atomic.c` reuses `worker()` from `main-common.c` and tests whether
  each `vector_add()` behaves as one indivisible operation (question 9).

---

### 1. Understanding the programs: `-n 2 -l 1 -v`

`main()` sets up `v[0..199]` (vector `i` has every element equal to `i`).
It starts `-n` threads that each run `worker()`, which calls
`vector_add(&v[v0], &v[v1])` `-l` times. With `-v`, `print_info()` prints
`->add(a, b)` just before the call and `<-add(a, b)` just after it. Each
thread's lines are indented 14 spaces per thread id, so thread 0 is in
column 0 and thread 1 is one step in. `vector-deadlock.c`'s `vector_add()`
locks `dst` and then `src`, adds the vectors element by element, and unlocks
them.

```
./vector-deadlock -n 2 -l 1 -v
```

```
->add(0, 1)
<-add(0, 1)
              ->add(0, 1)
              <-add(0, 1)
```

and on another run:

```
->add(0, 1)
              ->add(0, 1)
              <-add(0, 1)
<-add(0, 1)
```

Without `-d`, both threads call `add(0, 1)`. The output changes from run to
run only in how the threads' lines interleave, which depends on the
scheduler. I ran it 500 times and counted each interleaving (`data/q1.txt`;
`|` is a newline, and a leading space means a thread-1 line):

```
    456 ->add(0, 1)|<-add(0, 1)| ->add(0, 1)| <-add(0, 1)|
     27  ->add(0, 1)| <-add(0, 1)|->add(0, 1)|<-add(0, 1)|
     17 ->add(0, 1)| ->add(0, 1)| <-add(0, 1)|<-add(0, 1)|
```

Most of the time thread 0 finishes its single add before thread 1 has even
been created (`pthread_create` is slow compared with 100 additions). About 5%
of the time thread 1 runs first, and about 3% of the time both are "in" `add`
at once. In that case one of them is blocked on a lock: the lock serializes
the actual work, but the printouts overlap. It never deadlocks, because both
threads take the locks in the same order (`v[0]` then `v[1]`).

---

### 2. Adding `-d` and raising `-l`

What `-d` does in `main-common.c`:

```
if (cause_deadlock && i % 2 == 1)
    args[i].vector_add_order = 1;
```

Odd-numbered threads swap their two vectors, so thread 0 calls `add(0, 1)`
(locks `v[0]`, then `v[1]`) and thread 1 calls `add(1, 0)` (locks `v[1]`, then
`v[0]`). This is the classic Ch32 deadlock: if each thread gets its first lock
before the other gets its second, there is a cycle.

```
./vector-deadlock -n 2 -l 3 -d -v
```

```
->add(0, 1)
<-add(0, 1)
...
              ->add(1, 0)
              <-add(1, 0)
```

With only 3 loops, thread 0 is usually done before thread 1 starts, so the
cycle never forms. With more loops the two threads overlap, and the program
**hangs**. Here is a hung verbose run (`data/q2-hung-verbose.txt`, `stdbuf -oL`
so the killed process's output is not lost in its buffer):

```
# timeout 1 stdbuf -oL ./vector-deadlock -n 2 -l 100000 -d -v  (attempt 2, exit 124)
# last thread-0 line: 222678:->add(0, 1)
# last thread-1 line: 218088:              ->add(1, 0)
```

Both threads printed `->add` with no matching `<-add`. Thread 0 holds `v[0]`
and waits for `v[1]`, and thread 1 holds `v[1]` and waits for `v[0]`.

Fraction of 50 runs that hung (`timeout 1`; a normal run takes a few ms):

| -l | hung (with -d) | hung (without -d) |
|---|---|---|
| 1 | 0/50 | 0/50 |
| 10 | 0/50 | 0/50 |
| 100 | 2/50 | 0/50 |
| 1000 | 50/50 | 0/50 |
| 10000 | 50/50 | 0/50 |
| 100000 | 50/50 | 0/50 |

So it does **not always** deadlock. Deadlock needs a specific interleaving,
and the chance of that grows with how long the threads overlap. At `-l 1` it
practically never happens, while from `-l 1000` on it happens in almost
every run. This is why the chapter calls deadlocks hard to find: the code is
equally wrong at `-l 10`, but testing would show nothing.

---

### 3. Changing `-n`

Fraction of 30 runs of `./vector-deadlock -n N -l L -d` that hung
(`data/q3.txt`):

| -n | -l 1 | -l 10 | -l 100 | -l 1000 |
|---|---|---|---|---|
| 1 | 0/30 | 0/30 | 0/30 | 0/30 |
| 2 | 0/30 | 0/30 | 3/30 | 29/30 |
| 3 | 0/30 | 1/30 | 1/30 | 29/30 |
| 4 | 0/30 | 0/30 | 2/30 | 30/30 |
| 8 | 0/30 | 2/30 | 8/30 | 30/30 |
| 16 | 0/30 | 2/30 | 15/30 | 30/30 |
| 32 | 0/30 | 7/30 | 26/30 | 30/30 |

More threads mean more pairs of "forward" (even) and "backward" (odd) threads
running at the same time, so deadlock gets more likely at the same `-l`. At
`-l 100`, it goes from 3/30 with 2 threads to 26/30 with 32.

**`-n 1` is the only value that guarantees no deadlock.** With a single
thread, `-d` changes nothing (`i % 2 == 1` is never true for `i = 0`, so the
only thread always does `add(0, 1)`), and in any case one thread cannot
wait on itself in a cycle:

```
./vector-deadlock -n 1 -l 2 -d -v
->add(0, 1)
<-add(0, 1)
->add(0, 1)
<-add(0, 1)
```

Every `-n >= 2` can deadlock, given enough loops. (Without `-d`, every thread
uses the same order, so any `-n` is safe. With `-p`, each thread has its own
pair of vectors, so there is nothing to fight over. `-n 8 -l 100000 -d -p`
hung 0/30 times.)

---

### 4. vector-global-order.c

```
if (v_dst < v_src) { lock(dst); lock(src); }
else if (v_dst > v_src) { lock(src); lock(dst); }
else { lock(src); }   // same vector: lock it once
```

Every thread acquires the two locks in **increasing address order**, no
matter which vector is the destination. `add(0, 1)` and `add(1, 0)` both take
`v[0]` first, so a thread can only wait for a lock "higher" than every lock
it holds. A cycle in the waits-for graph would need some thread to wait for
a lower lock, which can't happen. This breaks the **circular wait**
condition (Ch32's "lock ordering", here like the Linux mm code, by lock
address).

The special case exists because pthread mutexes are not recursive. If
`dst == src`, the general code would call `lock(v->lock)` twice, and the
second call would wait forever for a lock the thread already holds: a
one-thread deadlock. It would also unlock the same mutex twice. Test with
`self-add.c` (`./run.sh q4`, `data/q4.txt`):

```
== self-add-deadlock
calling vector_add(&v[0], &v[0])...
exit=124                                <- hung, killed by timeout
== self-add-global-order
calling vector_add(&v[0], &v[0])...
returned; v[0].values[0] = 2
exit=0
```

(`main-common.c` never passes the same vector twice, so this case does not
come up in the homework runs, but a general-purpose `vector_add()` must
handle it.)

---

### 5. Timing global order: `-t -n 2 -l 100000 -d`

```
./vector-global-order -t -n 2 -l 100000 -d
Time: 0.01 seconds
```

It completes, with no deadlock, in about 10–20 ms. Sweeps (`data/q5.txt`,
median of 5):

| -l (n = 2) | time (s) |
|---|---|
| 100000 | 0.01 |
| 200000 | 0.03 |
| 400000 | 0.04 |
| 800000 | 0.07 |
| 1600000 | 0.16 |

| -n (l = 100000) | time (s) |
|---|---|
| 1 | 0.01 |
| 2 | 0.02 |
| 4 | 0.03 |
| 8 | 0.03 |
| 16 | 0.07 |
| 32 | 0.13 |

Time grows **linearly with loops** (doubling `-l` roughly doubles the time).
It also grows roughly **linearly with threads**: every thread needs the same
two locks, so the `n * l` additions run one at a time, and extra threads add
work without adding parallelism. With 1e6 loops (final table), 2 -> 16
threads goes 0.10 -> 0.63 s.

---

### 6. Turning on `-p`

With `-p`, thread `i` works on `v[2i]` and `v[2i+1]`, so no two threads share a
lock. The ideal result is that the time stays flat as threads are added,
because each thread does the same amount of work on its own core.

| -n (l = 100000, -d) | no -p | -p |
|---|---|---|
| 1 | 0.01 | 0.01 |
| 2 | 0.02 | 0.01 |
| 4 | 0.04 | 0.01 |
| 8 | 0.04 | 0.02 |
| 16 | 0.07 | 0.02 |
| 32 | 0.13 | 0.03 |

At 1e6 loops (`final`), 16 threads take 0.63 s without `-p` and 0.18 s with
it. The difference grows with `-n`: with no sharing, the threads really run in
parallel. I expected `-p` to be close to flat, but it isn't quite, so I
looked closer (`./run.sh knee`, `data/knee.txt`, `-l 1000000`):

| -n | global-order -p | global-order -p -d | nolock -p -d |
|---|---|---|---|
| 1 | 0.04 | 0.04 | 0.12 |
| 2 | 0.05 | 0.05 | 0.12 |
| 4 | 0.05 | 0.09 | 0.12 |
| 6 | 0.05 | 0.09 | 0.13 |
| 7 | 0.07 | 0.11 | 0.12 |
| 8 | 0.09 | 0.12 | 0.12 |
| 12 | 0.11 | 0.16 | 0.12 |
| 16 | 0.14 | 0.18 | 0.12 |

Two effects show up:

- **A knee after 6 threads** (first column). Up to 6 threads the time is flat;
  after that it rises. The host has 6 performance cores, and the VM cannot
  see which host core runs each vCPU, so I believe (but cannot prove from
  inside the VM) that threads 7+ end up on the slower efficiency cores. The
  total time is set by the slowest thread.
- **False sharing with `-d`** (second column, from `-n 4` on). `sizeof(vector_t)`
  is 448 (a 48-byte mutex + 400 bytes of ints) and `v` is at `0x20068`, so
  the array is not cache-line aligned. The last 24 bytes of `v[2i+1]` share
  a 64-byte line with the start of `v[2i+2]`'s mutex. With `-d`, odd threads
  *write* `v[2i+1]` (it is their destination), while the next thread locks
  `v[2i+2]` constantly, so the line bounces between cores even though the
  threads share no data. With `-n 2` the neighbor `v[4]` is unused, which is
  why that row shows no slowdown. `nolock` never touches the mutexes, so it
  stays flat.

---

### 7. vector-try-wait.c

```
top:
    if (pthread_mutex_trylock(&v_dst->lock) != 0) goto top;
    if (pthread_mutex_trylock(&v_src->lock) != 0) {
        retry++; unlock(&v_dst->lock); goto top;
    }
```

A thread never blocks while holding a lock. If it can't get the second lock,
it releases the first and starts over. That breaks **hold-and-wait** in the
blocking sense, and is close to adding **preemption** (the thread gives up
the lock itself), so no deadlock cycle can form.

**Is the first trylock needed?** No. When the thread reaches the first
acquire, it holds no locks, so blocking there cannot cause a deadlock. A plain
`pthread_mutex_lock(&v_dst->lock)` would be just as safe, and better: the
waiter would sleep instead of spinning. Only the *second* acquire, made while
holding a lock, has to be a trylock. As written, the first `goto top` is a
busy-wait spin lock built out of trylock, and its spins are not even counted
in `retry`.

Results (`data/q7.txt`, `-l 100000 -d`, median of 5, 60 s limit per run):

| -n | time (s) | retries | time -p | retries -p |
|---|---|---|---|---|
| 1 | 0.01 | 0 | 0.01 | 0 |
| 2 | 0.03 | 41533 | 0.01 | 0 |
| 4 | 3.87 | 25959857 | 0.01 | 0 |
| 8 | 19.19 | 74282725 | 0.02 | 0 |
| 16 | 51.19 | 132554938 | 0.03 | 0 |
| 32 | >60 | - | 0.03 | 0 |

Compared with global order (0.02 / 0.03 / 0.03 / 0.07 s for -n 2 / 4 / 8 / 16):

- With 2 threads it is about as fast (0.03 vs 0.02 s), with ~0.2 retries per add.
- From 4 threads on, it **collapses**: 3.87 s vs 0.03 s at `-n 4` (~100x),
  51 s at `-n 16`, and more than 60 s at `-n 32`. Retries go from 41 thousand
  to 133 million, about 65–93 failed second-lock attempts **per add**. The
  threads are not deadlocked, but they are close to **livelock**. Forward
  and backward threads keep grabbing one lock each, failing on the second,
  and releasing, and the spinning trylocks keep pulling the lock's cache
  line away from the thread that is trying to make progress.
- With `-p` nobody ever contends, so retries are 0 and it runs as fast as
  global order.

(`retry++` is not atomic, so with many threads the printed count is a lower
bound: concurrent increments are lost.)

---

### 8. vector-avoid-hold-and-wait.c

```
lock(&global);
lock(&v_dst->lock); lock(&v_src->lock);
unlock(&global);
... add ...
```

A global "acquisition" lock makes taking both locks one atomic step, so no
thread can hold one vector lock while another thread holds the other in the
middle of acquiring them. That breaks **hold-and-wait** (as it applies to
the vector locks).

**Main problem:** the global lock is shared by *every* call, even calls on
completely unrelated vectors, so it cuts concurrency. It is also worse than
it looks: the thread holding `global` can block inside it on a vector lock
that someone else is using, and then every other thread, including ones that
want different vectors, waits behind it. You also have to know all the locks
you need up front, which hurts encapsulation (the chapter's point).

| -n (l = 100000, -d) | no -p | -p |
|---|---|---|
| 1 | 0.01 | 0.01 |
| 2 | 0.02 | 0.03 |
| 4 | 0.03 | 0.04 |
| 8 | 0.04 | 0.09 |
| 16 | 0.07 | 0.21 |
| 32 | 0.14 | 0.44 |

- **Without `-p`** it is about the same as global order (0.64 vs 0.63 s at
  `-n 16 -l 1000000`). All threads already serialize on `v[0]`/`v[1]`, so one
  more serializing lock costs little.
- **With `-p`** it is by far the worst locking version, and slower than its own
  no-`-p` run: 2.04 s at `-n 16 -l 1000000`, vs 0.18 s for global order and
  try-wait. Every add, from every thread, has to pass through `global`,
  whose critical section is tiny. So the lock is handed between 16 threads
  millions of times (futex sleeps and wakeups, plus the `global` cache line
  moving between cores), while the real work could have run in parallel.
  This is the "one big lock" serialization from Ch29.

---

### 9. vector-nolock.c: same semantics?

```
for (i = 0; i < VECTOR_SIZE; i++)
    fetch_and_add(&v_dst->values[i], v_src->values[i]);
```

**No.** Each *element* update is atomic, so no single add is lost, but the
**whole-vector add is not atomic**. Two concurrent `vector_add()`s can
interleave element by element, and the read of `v_src->values[i]` is a plain
load that is not atomic with the add into `dst`. The locked versions behave
as if each `vector_add()` happened all at once, in some order.

Test (`check-atomic.c`, `./run.sh q9`, `data/q9.txt`): `v[0]` starts all-0 and
`v[1]` starts all-1. If every `vector_add()` is atomic, each element sees the
same sequence of adds, so each vector stays **uniform** (all 100 elements
equal). Threads use the `-d` pattern (`add(0,1)` and `add(1,0)` alternate),
with 20 loops each and 1000 trials:

```
== check-atomic-global-order n=2
non-uniform results: 0/1000 (n=2, l=20)
...                                         (try-wait, avoid-hold-and-wait: 0 too)
== check-atomic-nolock n=2
first non-uniform trial 0: v0[0]=501479 v0[99]=501864
non-uniform results: 78/1000 (n=2, l=20)
== check-atomic-nolock n=4
non-uniform results: 309/1000 (n=4, l=20)
== check-atomic-nolock n=8
non-uniform results: 255/1000 (n=8, l=20)
```

All three lock-based variants give 0 non-uniform results at `n = 2, 4, 8`.
nolock gives a vector whose elements disagree (for example `v0[0]=501479`
but `v0[99]=501864`) in 8–31% of trials. That state cannot be reached by any
serial order of whole-vector adds. nolock still never deadlocks, because it
holds no locks, so **mutual exclusion** itself is gone. It gives up
whole-vector atomicity in exchange.

---

### 10. nolock performance

From the final table below (`-l 1000000`, median of 5):

| -n | nolock same | global-order same | nolock -p | global-order -p |
|---|---|---|---|---|
| 2 | 0.38 | 0.10 | 0.12 | 0.05 |
| 4 | 3.09 | 0.21 | 0.12 | 0.08 |
| 8 | 4.99 | 0.34 | 0.12 | 0.12 |
| 16 | 14.00 | 0.63 | 0.13 | 0.18 |

- **Same two vectors (no `-p`): nolock is by far the slowest**, 4–22x slower
  than global order. A locked add pays for one contended lock handoff and then
  does 100 adds in the holder's own cache. nolock does 100 *contended*
  atomic read-modify-writes per add. Every `ldaddal` on `v[0]`/`v[1]` has to
  take the cache line exclusively, and with several cores doing this at once
  the lines move between cores on almost every element. Removing the lock
  did not remove the sharing.
- **Separate vectors (`-p`): nolock scales perfectly** (0.12 s from 1 to 16
  threads, as in the knee table), because it only touches its own data. For
  a single thread it costs about 2–3x a locked add (0.12 vs 0.04–0.05 s). That
  is 100 `ldaddal` calls (through the outline-atomics helper, with
  acquire/release ordering) instead of plain `ldr`/`add`/`str`. By `-n 16` it
  is the fastest variant, because the locking versions slow down past 6
  threads (question 6).

---

### Final comparison (all five variants)

`TLIM=30 ./run.sh final` (`data/final.txt`): `-t -l 1000000`, median of 5 (s).
`-d` is passed to every variant except `vector-deadlock` without `-p`, which
would hang. With `-p`, `-d` is harmless for it, because each thread has its
own vectors. ">30" means a run exceeded 30 s.

**Same two vectors (no `-p`):**

| variant | -n 2 | -n 4 | -n 8 | -n 16 |
|---|---|---|---|---|
| vector-deadlock (no -d) | 0.11 | 0.19 | 0.32 | 0.59 |
| vector-global-order | 0.10 | 0.21 | 0.34 | 0.63 |
| vector-try-wait | 0.16 | >30 | >30 | >30 |
| vector-avoid-hold-and-wait | 0.13 | 0.17 | 0.32 | 0.64 |
| vector-nolock | 0.38 | 3.09 | 4.99 | 14.00 |

**Separate vectors (`-p`):**

| variant | -n 2 | -n 4 | -n 8 | -n 16 |
|---|---|---|---|---|
| vector-deadlock | 0.05 | 0.09 | 0.12 | 0.18 |
| vector-global-order | 0.05 | 0.08 | 0.12 | 0.18 |
| vector-try-wait | 0.05 | 0.09 | 0.13 | 0.18 |
| vector-avoid-hold-and-wait | 0.17 | 0.32 | 0.86 | 2.04 |
| vector-nolock | 0.12 | 0.12 | 0.12 | 0.13 |

Which deadlock condition each variant breaks:

| variant | mutual excl. | hold-and-wait | no preemption | circular wait |
|---|---|---|---|---|
| deadlock | kept | kept | kept | kept (with -d) -> **deadlocks** |
| global-order | kept | kept | kept | **broken** (address order) |
| try-wait | kept | **broken** (never blocks holding) | **broken** (releases) | possible, never blocks |
| avoid-hold-and-wait | kept | **broken** (atomic acquire) | kept | kept |
| nolock | **broken** (no locks) | n/a | n/a | n/a |

Takeaways:

- With contention (no `-p`), the ordering-based variant (global order) is
  the best correct option. Avoid-hold-and-wait matches it only because
  everything was serialized anyway. Try-wait livelocks, and nolock
  ping-pongs cache lines.
- Without contention (`-p`), anything with per-vector locks is fine, the
  global lock is a disaster, and nolock is the only variant that stays flat.
- The cheapest condition to break is **circular wait**, by a total lock
  order. It costs nothing when there is no contention and keeps the full
  semantics.

---

## Connections to earlier chapters

- **Lock ordering (Ch29, Ch32).** Global order by address is the same idea
  as the fixed top-down order used in hand-over-hand locking of lists and
  B-trees in Ch29. As long as every thread acquires locks in one agreed
  direction (root to leaf, or low address to high), a waits-for cycle is
  impossible. The `dst == src` special case is the price of ordering by
  address: the order must be strict, and the same lock cannot appear twice.
- **Trylock retry = spinning (Ch28) and the scheduler (Ch7–8).**
  `vector-try-wait`'s `goto top` loops are spin locks. Like test-and-set,
  they waste CPU while another thread holds the lock, and with many threads
  they turn into near-livelock (question 7). They depend on the scheduler:
  with 18 vCPUs the holder is usually running, but a spinner that burns its
  time slice while the holder is preempted is the Ch28 problem again. The
  usual fixes are the same: `sched_yield()` or random backoff before
  retrying, or a blocking lock for the first acquire.
- **One big lock (Ch29).** Avoid-hold-and-wait's `global` mutex is a
  coarse-grained lock. It is simple and correct, but like the single-lock
  counter or list in Ch29 it serializes unrelated operations. Question 8
  with `-p` (0.18 s -> 2.04 s at 16 threads) is the same "doesn't scale"
  result.
- **Fetch-and-add (Ch28).** nolock relies on the same hardware primitive as
  the ticket lock's `FetchAndAdd`. On x86 that is `lock xadd`, and here ARM
  LSE `ldaddal`. Hardware makes one word's read-modify-write atomic. A
  100-word vector update is still 100 separate atomic operations
  (question 9), so getting atomicity over the whole vector needs a lock (or a
  different design).
- **Multiprocessor caches (Ch10) and scalability (Ch29).** `-p` is the
  cache-affinity experiment. Shared vectors force cache lines to move
  between cores, private ones stay in each core's cache. Ch29's measurement
  that a shared counter doesn't scale, even with atomic increments, is
  exactly nolock without `-p` (14 s at 16 threads). False sharing between
  `v[2i+1]` and `v[2i+2]`'s mutex (question 6) shows that "different data"
  is not the same as "different cache lines".
- **Deadlock with semaphores and condition variables (Ch30–31).** The same
  four conditions apply to any blocking primitive, not just mutexes. Ch31's
  bounded buffer deadlocks if the producer takes the mutex and then
  `sem_wait`s on `empty` (hold-and-wait plus a cycle with the consumer). The
  fix, taking `empty`/`full` before the mutex, is a lock-ordering rule.
  Ch32's dining philosophers deadlock in the same circular-wait way as
  `add(0,1)` and `add(1,0)`, and changing one philosopher's fork order is the
  same fix as global order.
