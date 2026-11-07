# Chapter 28 — Locks

Simulator: `x86.py` (copied unmodified from the upstream `threads-locks/`
directory, together with the `.s` programs), run as `python3 x86.py ...`. Each
thread runs the same program; `-a` sets per-thread registers, `-i` is the
interrupt (time-slice) length in instructions, `-P` gives an exact schedule,
`-M`/`-R` trace memory/registers, `-S` prints an instruction count, and `-c`
computes the values. Every output below is real, trimmed with `...`; notes in
parentheses or after `<-` inside output blocks are my annotations.

All programs loop with `sub $1,%bx; test $0,%bx; jgt .top`, so a thread with
`bx=N` (N >= 1) enters the critical section N times; `bx=0` (the default) still
enters once. The shared counter `count` is the correctness check: with T
threads it must end at T x N.

For the sweeps I use `./sweep.sh`:

```
[PC=addr] ./sweep.sh PROG THREADS BX "I_LIST"
```

It prints one row per `-i` value: the final `count`, the expected value, the
total instructions executed (`-S`), the **ideal** count (the same work done by
one thread with no contention, plus the extra halts), **overhead** = total -
ideal (instructions burned spinning or retrying), the number of `xchg`
instructions executed, and how often address `PC` (default 1000, the first
acquire instruction) ran.

---

### 1. Understanding flag.s

```
.acquire
mov  flag, %ax      # load the flag
test $0, %ax        # is it 0 (free)?
jne  .acquire       # no: spin
mov  $1, flag       # yes: set it to 1 ("take the lock")
mov  count, %ax     # critical section: count = count + 1
add  $1, %ax
mov  %ax, count
mov  $0, flag       # release
sub  $1, %bx        # loop bx times
test $0, %bx
jgt .top
halt
```

Acquire spins until `flag` reads 0, then writes 1. Release writes 0. The bug
is that the **test** (`mov flag,%ax` + `test` + `jne`) and the **set**
(`mov $1,flag`) are separate instructions. If a thread is interrupted after
it reads 0 but before it stores 1, another thread can also read 0, and both
enter the critical section. This is the first failed attempt in the chapter
(Figure 28.1). One loop iteration is 11 instructions (addresses 1000–1010).

---

### 2. Default run: does it work?

```
python3 x86.py -p flag.s -M flag,count -R ax,bx -c
```

```
 flag count      ax    bx          Thread 0                Thread 1
    0     0       0     0
    0     0       0     0   1000 mov  flag, %ax
    0     0       0     0   1001 test $0, %ax
    0     0       0     0   1002 jne  .acquire
    1     0       0     0   1003 mov  $1, flag
    1     0       1     0   1004 mov  count, %ax
    1     0       1     0   1005 add  $1, %ax
    1     1       1     0   1006 mov  %ax, count
    0     1       1     0   1007 mov  $0, flag
    0     1       1    -1   1008 sub  $1, %bx
    ...
    0     1       1    -1   1011 halt
    0     1       0     0   ----- Halt;Switch -----  ----- Halt;Switch -----
    0     1       0     0                            1000 mov  flag, %ax
    ...
    1     2       2     0                            1006 mov  %ax, count
    0     2       2     0                            1007 mov  $0, flag
    ...
```

Yes, it works, but only because nothing interrupts it. With the default
interval (`-i 50`), each thread needs only 12 instructions, so thread 0
finishes before thread 1 starts. Prediction: **`flag` ends at 0**, because
every thread clears it on release, and `count` ends at 2.

---

### 3. Looping with -a bx=2,bx=2

```
python3 x86.py -p flag.s -M flag,count -R ax,bx -c -a bx=2,bx=2
```

```
 flag count      ax    bx          Thread 0                Thread 1
    0     0       0     2
    0     0       0     2   1000 mov  flag, %ax
    ...
    1     1       1     2   1006 mov  %ax, count
    0     1       1     2   1007 mov  $0, flag
    0     1       1     1   1008 sub  $1, %bx
    0     1       1     1   1009 test $0, %bx
    0     1       1     1   1010 jgt .top
    0     1       0     1   1000 mov  flag, %ax
    ...
```

`bx` is a loop counter: each thread acquires the lock, increments `count`, and
releases it `bx` times. Each thread now runs 2 x 11 + 1 = 23 instructions,
which is still less than 50, so the threads still never overlap. `flag`
still ends at 0, and `count` ends at **4** (2 threads x 2). The answer only
changes once a thread runs long enough to be interrupted (question 4).

---

### 4. High bx, different interrupt intervals

```
./sweep.sh flag.s 2 100 "1 2 3 ... 1000"
```

Two threads, `bx=100` each, so the expected `count` is 200 (ideal = 2202
instructions).

| -i | count | expected | total instr | spin overhead | result |
|---|---|---|---|---|---|
| 1 | 100 | 200 | 2202 | 0 | bad |
| 2 | 100 | 200 | 2202 | 0 | bad |
| 3 | 133 | 200 | 2202 | 0 | bad |
| 4 | 167 | 200 | 2406 | 204 | bad |
| 5 | 175 | 200 | 2502 | 300 | bad |
| 7 | 166 | 200 | 2778 | 576 | bad |
| 10 | 193 | 200 | 2850 | 648 | bad |
| **11** | **200** | 200 | 2202 | 0 | good |
| 12 | 176 | 200 | 2202 | 0 | bad |
| 13 | 170 | 200 | 2202 | 0 | bad |
| 15 | 200 | 200 | 2997 | 795 | good (luck) |
| 17 | 194 | 200 | 3228 | 1026 | bad |
| 20 | 179 | 200 | 2661 | 459 | bad |
| 21 | 200 | 200 | 2955 | 753 | good (luck) |
| **22** | **200** | 200 | 2202 | 0 | good |
| 25 | 176 | 200 | 2202 | 0 | bad |
| **33, 44, 55** | **200** | 200 | 2202 | 0 | good |
| 50 | 185 | 200 | 2865 | 663 | bad |
| **66, 77, 88, 99, 110** | **200** | 200 | 2202 | 0 | good |
| 100 | 181 | 200 | 2202 | 0 | bad |
| 1000 | 200 | 200 | 2202 | 0 | good |

- **Bad outcomes:** any interval that sometimes switches threads inside the
  window between `mov flag,%ax` and `mov $1,flag` (or while a thread with a
  stale `count` in `%ax` is in the critical section). Both threads then see
  `flag == 0` and enter together, and one increment is lost. At `-i 1` the
  threads alternate on every instruction and *every* pair of increments
  collides, so `count` is exactly half (100).
- **Good outcomes:** multiples of 11, the loop length. Every switch then lands
  exactly at the loop boundary, after the lock is released and before it is
  read again, so the threads never meet. `-i 1000` works because the switch
  points fall outside the window. `-i 15` and `-i 21` give 200 by luck: the
  threads did overlap (there is spin overhead), but no update was lost.
- Even the "good" values are fragile. The same `-i 11` with random interrupt
  lengths (`-r`) fails for every seed I tried:

```
for s in 0 1 2 3 4; do python3 x86.py -p flag.s -a bx=100,bx=100 -i 11 -r -s $s \
  -M count -c | tail -1; done
```

```
  183                            1011 halt
  185   1011 halt
  184   1011 halt
  181                            1011 halt
  192   1011 halt
```

A lock that only works for "lucky" time-slice lengths is not a lock. The
programmer controls neither the quantum nor when interrupts arrive.

---

### 5. Understanding test-and-set.s

```
.acquire
mov  $1, %ax
xchg %ax, mutex     # atomically: old = mutex; mutex = 1; ax = old
test $0, %ax        # was the old value 0?
jne  .acquire       # no: someone holds it, spin
...critical section...
mov  $0, mutex      # release
```

- **Acquire:** put 1 in `%ax` and atomically swap it with `mutex`. The
  `xchg` always leaves `mutex == 1`, and `%ax` holds the *old* value. If the
  old value was 0, the lock was free and we now own it. If it was 1, someone
  else owns it and nothing changed, so we try again. Test and set happen in
  **one atomic instruction**, which closes the window from flag.s.
- **Release:** a plain store `mov $0, mutex`. Only the owner runs it, so it
  needs no atomic instruction.

This is the spin lock built from `TestAndSet` in Figure 28.3.

---

### 6. Does it always work? Is the CPU used efficiently?

```
./sweep.sh test-and-set.s 2 100 "1 2 3 4 5 6 7 8 9 10 11 12 15 20 50 100 1000"
```

| -i | count | expected | total | ideal | overhead | % wasted | acquire attempts |
|---|---|---|---|---|---|---|---|
| 1 | 200 | 200 | 3002 | 2202 | 800 | 27% | 400 |
| 2 | 200 | 200 | 3394 | 2202 | 1192 | 35% | 498 |
| 5 | 200 | 200 | 2998 | 2202 | 796 | 27% | 399 |
| 8 | 200 | 200 | 2810 | 2202 | 608 | 22% | 352 |
| 10 | 200 | 200 | 2986 | 2202 | 784 | 26% | 396 |
| 11 | 200 | 200 | 2202 | 2202 | 0 | 0% | 200 |
| 12 | 200 | 200 | 3858 | 2202 | 1656 | 43% | 614 |
| 15 | 200 | 200 | 3538 | 2202 | 1336 | 38% | 534 |
| 20 | 200 | 200 | 3010 | 2202 | 808 | 27% | 402 |
| 50 | 200 | 200 | 3354 | 2202 | 1152 | 34% | 488 |
| 100 | 200 | 200 | 3594 | 2202 | 1392 | 39% | 548 |
| 1000 | 200 | 200 | 2202 | 2202 | 0 | 0% | 200 |

**Correctness:** `count` is 200 for every interval, so the lock works.

**Efficiency:** it often wastes a lot of CPU. When the lock holder is
preempted inside the critical section, the other thread spins for its
*entire* time slice:

```
python3 x86.py -p test-and-set.s -a bx=100,bx=100 -i 50 -M mutex,count -R ax -c
```

```
mutex count      ax          Thread 0                Thread 1
...
    1     4       0   ------ Interrupt ------  ------ Interrupt ------
    1     4       1                            1000 mov  $1, %ax
    1     4       1                            1001 xchg %ax, mutex
    1     4       1                            1002 test $0, %ax
    1     4       1                            1003 jne  .acquire
    1     4       1                            1000 mov  $1, %ax
    ...   (the same 4 instructions repeat until the quantum ends)
```

**How to quantify it:** count acquire attempts. Each successful acquire
executes address 1000 once, and every failed attempt executes the 4-instruction
loop (1000–1003) once more. So

```
wasted instructions = 4 x (executions of PC 1000 - number of acquires)
```

For `-i 50`: 4 x (488 - 200) = 1152, which matches total - ideal = 3354 -
2202. About a third of the CPU went to spinning. The waste is zero only when
the quantum is an exact multiple of the 11-instruction loop (or longer than the
whole job), so a thread is never preempted while holding the lock.

---

### 7. Targeted schedules with -P

`-P` lists which thread runs each instruction (use `-i 1` so a switch can
happen after every instruction). **Case 1:** thread 0 takes the lock and is
preempted inside the critical section; thread 1 then tries to acquire it:

```
python3 x86.py -p test-and-set.s -i 1 -P 00000111111110000000111111111111111 \
  -M mutex,count -R ax -c
```

```
mutex count      ax          Thread 0                Thread 1
    0     0       1   1000 mov  $1, %ax
    1     0       0   1001 xchg %ax, mutex
    1     0       0   1002 test $0, %ax
    1     0       0   1003 jne  .acquire
    1     0       0   1004 mov  count, %ax
    1     0       0   ------ Interrupt ------  ------ Interrupt ------
    1     0       1                            1000 mov  $1, %ax
    1     0       1                            1001 xchg %ax, mutex
    1     0       1                            1002 test $0, %ax
    1     0       1                            1003 jne  .acquire
    1     0       1                            1000 mov  $1, %ax
    ...
    1     0       0   1005 add  $1, %ax
    1     1       1   1006 mov  %ax, count
    0     1       1   1007 mov  $0, mutex
    ...
    0     1       1                            1000 mov  $1, %ax
    1     1       0                            1001 xchg %ax, mutex
    ...
    1     2       2                            1006 mov  %ax, count
```

Yes, the right thing happens. Thread 1's `xchg` gets back 1, so it spins, and
it only gets in after thread 0 stores 0. Final `count` = 2. For comparison,
the same kind of schedule breaks flag.s. Thread 0 is preempted right after its
`test`/`jne` saw 0:

```
python3 x86.py -p flag.s -i 1 -P 00011111000000000000001111111111 -M flag,count -R ax -c
```

```
 flag count      ax          Thread 0                Thread 1
    0     0       0   1002 jne  .acquire
    0     0       0                            1002 jne  .acquire
    1     0       0                            1003 mov  $1, flag
    1     0       0                            1004 mov  count, %ax
    1     0       0   1003 mov  $1, flag          <- both are inside now
    ...
    1     1       1   1006 mov  %ax, count
    0     1       1                            1006 mov  %ax, count  <- count = 1 !
```

**Case 2:** preemption right *after* `xchg` but before `test`. Thread 0
already owns the lock but has not noticed yet:

```
python3 x86.py -p test-and-set.s -i 1 -P 00111111111000000000000011111111111111 \
  -M mutex,count -R ax -c
```

```
    0     0       1   1000 mov  $1, %ax
    1     0       0   1001 xchg %ax, mutex
    1     0       0   ------ Interrupt ------  ------ Interrupt ------
    1     0       1                            1000 mov  $1, %ax
    1     0       1                            1001 xchg %ax, mutex
    1     0       1                            1002 test $0, %ax
    1     0       1                            1003 jne  .acquire
    ...
    1     0       0   1002 test $0, %ax
    1     0       0   1003 jne  .acquire
    1     0       0   1004 mov  count, %ax
```

This is correct as well: the swap itself is the moment of acquisition, so it
does not matter where the thread is preempted afterwards.

**Other schedules to test.** I also ran a set of repeating schedules with
`bx=50` per thread, so the expected count is 100:

```
python3 x86.py -p test-and-set.s -a bx=50,bx=50 -i 1 -P <pattern> -M count -c -S
```

| -P pattern | test-and-set.s count | total instr | flag.s count |
|---|---|---|---|
| `01` (switch every instruction) | 100 | 1502 | 50 |
| `0011` | 100 | 1694 | 50 |
| `001` | 100 | 1646 | 100 |
| `0001111` | 100 | 1598 | 75 |
| `01101` | 100 | 1742 | 82 |
| `000000011` | 100 | 1726 | 87 |

Things that should be tested:

- both threads reach `xchg` at the same time (`-P 01`): exactly one wins;
- preemption at every point of acquire, inside the critical section, and in
  the middle of release;
- the holder releasing and immediately re-acquiring. This is allowed, but it
  shows that test-and-set is **not fair**: a thread can win again and again
  while the other starves (questions 11–12);
- more than two threads (`-t 3`, `-t 5`): `count` was always correct there too.

---

### 8. Understanding peterson.s

```
lea flag, %fx           # fx = &flag[0]
mov %bx, %cx; neg %cx; add $1, %cx   # cx = 1 - self  (the other thread)
.acquire
mov $1, 0(%fx,%bx,4)    # flag[self] = 1      "I want in"
mov %cx, turn           # turn = other        "but you go first"
.spin1
mov 0(%fx,%cx,4), %ax   # if flag[other] != 1 -> enter
test $1, %ax
jne .fini
mov turn, %ax           # else if turn == other -> keep spinning
test %cx, %ax
je .spin1
.fini
...critical section...
mov $0, 0(%fx,%bx,4)    # release: flag[self] = 0
mov %cx, turn
```

This is Peterson's algorithm from the chapter sidebar, which uses only plain
loads and stores. A thread waits only while **both** (a) the other thread
wants in and (b) it is the other thread's turn. Each thread raises its flag
and then politely gives the turn away. If both do this, whoever wrote `turn`
*last* loses and waits. `flag` is a two-element array (`flag[0]` at address
100, `flag[1]` at 104), indexed with scale 4. The thread ID must be in `%bx`.
Each thread enters the critical section only once (there is no loop). An
uncontended thread runs 15 instructions.

---

### 9. Peterson with different -i

```
python3 x86.py -p peterson.s -a bx=0,bx=1 -i $i -M count -c -S
```

| -i | final count | total instr | `spin1` loads (PC 1006) |
|---|---|---|---|
| 1 | 2 | 45 | 4 |
| 2 | 2 | 45 | 4 |
| 3 | 2 | 42 | 3 |
| 4 | 2 | 36 | 3 |
| 5 | 2 | 54 | 5 |
| 6 | 2 | 39 | 3 |
| 7, 8, 9 | 2 | 33 | 2 |
| 10, 12 | 2 | 36 | 3 |
| 15, 20, 50 | 2 | 30 | 2 |

- For `-i >= 15` each thread finishes before the other starts: 30
  instructions, with no waiting.
- For small `-i` the threads interleave. Both raise their flags, one of them
  spins (extra loads of `flag[other]` and `turn`), and the run costs up to 54
  instructions. `count` is still always 2: the spinning costs time but never
  breaks mutual exclusion. Random interrupts (`-i 3 -r`, seeds 0–9) also gave
  `count` = 2 every time (33–48 instructions).
- The thread IDs matter. With `-a bx=0,bx=0` both threads use `flag[0]` and
  think the other is thread 1, so the algorithm breaks: `count` = 1 at
  `-i 1`, `2` and `5`.

---

### 10. "Proving" Peterson with -P

Mutual exclusion and deadlock avoidance come down to a few cases. Below,
columns `100`/`104` are `flag[0]`/`flag[1]` (interrupt lines removed).

**Case A: both threads want in at once; the last writer of `turn` waits.**
Thread 0 writes turn=1, then thread 1 writes turn=0, so thread 0 enters and
thread 1 spins:

```
python3 x86.py -p peterson.s -a bx=0,bx=1 -i 1 \
  -P 000000111111000000011111111111100000111111111111111 -M 100,104,turn,count -c
```

```
  100   104  turn count          Thread 0                Thread 1
    1     0     1     0   1005 mov %cx, turn
    ...
    1     1     0     0                            1005 mov %cx, turn
    1     1     0     0   1006 mov 0(%fx,%cx,4), %ax
    ...
    1     1     0     0   1011 je .spin1
    1     1     0     0   1012 mov count, %ax          (T0 in critical section)
    1     1     0     0                            1006 mov 0(%fx,%cx,4), %ax
    ...
    1     1     0     0                            1011 je .spin1   (T1 spins)
    ...
    1     1     0     0   1013 add $1, %ax
    1     1     0     1   1014 mov %ax, count
    0     1     0     1   1015 mov $0, 0(%fx,%bx,4)
    ...
    0     1     1     1                            1008 jne .fini   (now T1 enters)
    0     1     1     2                            1014 mov %ax, count
```

**Case B: the reverse order.** Thread 0 raises its flag, thread 1 runs its
whole entry protocol (turn=0), and *then* thread 0 writes turn=1. Now thread 0
is the last writer, so it spins and thread 1 enters:

```
python3 x86.py -p peterson.s -a bx=0,bx=1 -i 1 \
  -P 0000011111100000001111111111111111111111111111 -M 100,104,turn,count -c
```

```
    1     1     0     0                            1005 mov %cx, turn
    1     1     1     0   1005 mov %cx, turn
    ...
    1     1     1     0   1011 je .spin1                 (T0 spins)
    ...
    1     1     1     0                            1012 mov count, %ax
    1     1     1     1                            1014 mov %ax, count
    1     0     1     1                            1015 mov $0, 0(%fx,%bx,4)
    ...
    1     0     0     1   1008 jne .fini                 (T0 enters)
    0     0     1     2   1016 mov %cx, turn
```

**Case C: one thread is already inside when the other arrives.** Thread 0
gets in uncontested; thread 1 must wait until `flag[0]` goes back to 0:

```
python3 x86.py -p peterson.s -a bx=0,bx=1 -i 1 \
  -P 00000000001111111111110000011111111111111111 -M 100,104,turn,count -c
```

```
    1     0     1     0   1008 jne .fini           (flag[1]==0: enter directly)
    1     0     1     0   1012 mov count, %ax
    ...
    1     1     0     0                            1011 je .spin1   (T1 waits)
    1     1     0     1   1014 mov %ax, count
    0     1     0     1   1015 mov $0, 0(%fx,%bx,4)
    ...
    0     1     1     2                            1014 mov %ax, count
```

What these cases show:

- **Mutual exclusion:** two threads can only be inside together if each saw
  either `flag[other] == 0` or `turn == self`. If both flags are up, `turn`
  holds a single value, so only one thread can pass (cases A and B). If one
  thread is already inside, its flag stays up, and the newcomer gave the turn
  away, so it waits (case C).
- **No deadlock (progress):** both threads can never spin forever, because
  `turn` always favors one of them (cases A and B). A thread alone never
  waits at all.
- **Bounded waiting:** a waiting thread gets in after at most one entry by
  the other thread, because the other thread's next entry sets `turn` in the
  waiter's favor.

The test cases to cover are therefore: no contention, both arrive with T0 last
to write `turn`, both arrive with T1 last, and one already inside (for each
thread).

---

### 11. The ticket lock

```
.acquire
mov $1, %ax
fetchadd %ax, ticket  # myturn = FetchAndAdd(&ticket, 1)
.tryagain
mov turn, %cx         # while (turn != myturn) spin
test %cx, %ax
jne .tryagain
...critical section...
mov $1, %ax
fetchadd %ax, turn    # release: turn = turn + 1
```

This matches Figure 28.7: `ticket` plays the role of `lock->ticket` and
`turn` of `lock->turn`. Acquire atomically takes the next ticket and spins
until `turn` equals it. Release hands the lock to the next ticket. One
iteration is 13 instructions.

```
python3 x86.py -p ticket.s -a bx=1000,bx=1000 -M count,ticket,turn -c -S
```

```
 99462  2000  2000  2000                            1013 halt

STATS:: Instructions    99463
```

The lock is correct (`count` = 2000), but the threads spend **most of their
time spin-waiting**. Each full pass through the critical section is
13 x 2000 + 2 = 26002 instructions of useful work, yet the run took 99463.
In steady state every 50-instruction quantum looks like this:

```
python3 x86.py -p ticket.s -a bx=1000,bx=1000 -M ticket,turn,count -R ax,cx -c -S
```

```
icount ticket  turn count      ax    cx          Thread 0                Thread 1
  1000    29    27    27      27    27   1002 mov turn, %cx
  ...
  1005    29    27    28      28    27   1007 mov  %ax, count
  1007    29    28    28      27    27   1009 fetchadd %ax, turn
  ...
  1012    30    28    28      29    27   1001 fetchadd %ax, ticket   (my ticket 29)
  1013    30    28    28      29    28   1002 mov turn, %cx
  1014    30    28    28      29    28   1003 test %cx, %ax
  1015    30    28    28      29    28   1004 jne .tryagain
  ...   (12 more spin iterations, turn stays 28)
  1049    30    28    28      29    28   1002 mov turn, %cx
  1050    30    28    28      28    27   ------ Interrupt ------  ------ Interrupt ------
```

After one critical section, thread 0 releases (turn=28), immediately takes
ticket 29, and must wait for ticket 28, which belongs to thread 1. Thread 1
is not running, so thread 0 spins for the rest of its slice. The lock forces
**strict alternation**, so each quantum does about one critical section and
roughly 37 instructions of spinning.

Quantified with the spin-loop address (`PC=1002`): wasted = 3 x
(executions of 1002 - 2000).

```
PC=1002 ./sweep.sh ticket.s 2 1000 "1 5 13 50 100"
```

| -i | count | total | ideal | overhead | % wasted | spin checks (PC 1002) |
|---|---|---|---|---|---|---|
| 1 | 2000 | 37999 | 26002 | 11997 | 32% | 5999 |
| 5 | 2000 | 30001 | 26002 | 3999 | 13% | 3333 |
| 13 | 2000 | 26002 | 26002 | 0 | 0% | 2000 |
| 50 | 2000 | 99463 | 26002 | 73461 | **74%** | 26487 |
| 100 | 2000 | 198718 | 26002 | 172716 | **87%** | 59572 |

The waste grows with the quantum: a longer slice just means a longer spin.
At `-i 50` the ticket lock wastes 74%, while test-and-set wastes 34% (question
6). Test-and-set lets the running thread re-acquire and keep going, whereas
the ticket lock's fairness forces a handoff to a thread that is not even on
the CPU. The only perfect case is a quantum of exactly one iteration (13).

---

### 12. More threads

`bx=100` per thread, `-t 2 ... 8`:

```
PC=1002 ./sweep.sh ticket.s $t 100 "1 13 50"
./sweep.sh test-and-set.s $t 100 "50"
```

| threads | ticket -i 1 wasted | ticket -i 13 wasted | ticket -i 50 total / wasted | TAS -i 50 total / wasted |
|---|---|---|---|---|
| 2 | 1197 (32%) | 0 | 9463 / 6861 (73%) | 3354 / 1152 (34%) |
| 3 | 4482 (53%) | 0 | 14424 / 10521 (73%) | 7287 / 3984 (55%) |
| 4 | 9558 (65%) | 0 | 19286 / 14082 (73%) | 11856 / 7452 (63%) |
| 5 | 16425 (72%) | 0 | 24148 / 17643 (73%) | 18893 / 13388 (71%) |
| 8 | 47772 (82%) | 0 | 38836 / 28428 (73%) | 45016 / 36208 (80%) |

- `count` was always correct (T x 100).
- With more threads, more of them are waiting at any moment, so total
  spinning grows: at `-i 1` waste goes from 32% to 82%. At `-i 50` the ticket
  lock stays at about 73%: each slice still gets one critical section, then
  the thread spins until the slice ends (the holder of the next ticket is
  asleep). With 8 threads the ticket lock actually wastes *less* than
  test-and-set, whose waste climbs as more threads burn whole slices on a
  held lock.
- The ticket lock is **fair**. After the first quantum (when everyone grabs
  tickets), threads enter in strict FIFO order. The order of critical-section
  entries with 3 threads, `bx=10`, `-i 50` (0/1/2 = thread):

```
ticket.s:        000011112222000012012012121212
test-and-set.s:  000000000111111111222222222012
```

Test-and-set gives the lock to whoever is running and leaves the others to
starve within a slice. The ticket lock rotates 0,1,2,0,1,2..., which
guarantees progress for every thread at the cost of spinning.

---

### 13. yield.s vs test-and-set.s

yield.s is test-and-set, but when `xchg` returns 1 it runs `yield` (give up
the CPU) instead of spinning. A failed attempt costs 6 instructions (`mov,
xchg, test, je, yield, j`) instead of looping until the slice ends.

**Scenario:** the lock holder is preempted inside its critical section. With
`bx=2,bx=2 -i 16`, thread 0 takes the lock on its second iteration, runs 1
instruction of the critical section, and is interrupted:

```
python3 x86.py -p test-and-set.s -a bx=2,bx=2 -i 16 -M mutex,count -c -S
python3 x86.py -p yield.s        -a bx=2,bx=2 -i 16 -M mutex,count -c -S
```

```
== test-and-set.s
    15     1     1   1004 mov  count, %ax
    16     1     1   ------ Interrupt ------  ------ Interrupt ------
    16     1     1                            1000 mov  $1, %ax
    17     1     1                            1001 xchg %ax, mutex
    ...   (4 failed attempts = the whole 16-instruction slice)
    31     1     1                            1003 jne  .acquire
    32     1     1   ------ Interrupt ------  ------ Interrupt ------
    32     1     1   1005 add  $1, %ax
...
STATS:: Instructions    62

== yield.s
    15     1     1   1006 mov  count, %ax
    16     1     1   ------ Interrupt ------  ------ Interrupt ------
    16     1     1                            1000 mov  $1, %ax
    17     1     1                            1001 xchg %ax, mutex
    18     1     1                            1002 test $0, %ax
    19     1     1                            1003 je .acquire_done
    20     1     1                            1004 yield
    21     1     1   ------ Interrupt ------  ------ Interrupt ------
    21     1     1   1007 add  $1, %ax
...
STATS:: Instructions    52
```

Test-and-set burns the whole 16-instruction slice. yield.s gives the CPU back
after 5 instructions (plus 1 `j` later): **10 instructions saved** (62 vs 52).
In general, each time a waiter is scheduled while the lock is held, yield saves
about (quantum - 6) instructions.

Sweeping (`bx=100` per thread, overhead = total - ideal):

```
./sweep.sh test-and-set.s $t 100 "1 5 10 20 50 100 200"
./sweep.sh yield.s        $t 100 "1 5 10 20 50 100 200"
```

| -i | 2 thr TAS | 2 thr yield | 3 thr TAS | 3 thr yield | 5 thr TAS | 5 thr yield |
|---|---|---|---|---|---|---|
| 1 | 800 | 1194 | 3172 | 3588 | 11268 | 11964 |
| 5 | 796 | 804 | 2696 | 2148 | 9472 | 6666 |
| 10 | 784 | 684 | 1832 | 1746 | 4256 | 4362 |
| 20 | 808 | 324 | 1984 | 804 | 4912 | 2034 |
| 50 | 1152 | 132 | 3984 | 396 | 13388 | 1320 |
| 100 | 1392 | 72 | 4288 | 144 | 14576 | 390 |
| 200 | 1392 | 36 | 4188 | 72 | 14380 | 204 |

**When the savings appear:** with long quanta and many threads (5 threads at
`-i 100`: 14576 wasted instructions become 390, 97% less; overall waste drops
from 73% to 7%). That is exactly when test-and-set wastes whole slices on a
preempted holder. **When they do not:** at very small quanta (`-i 1`) a switch
happens anyway, and yield's longer failure path (6 vs 4 instructions) makes it
slightly *worse*. Yield also does not remove the cost of the context switches
themselves, which the simulator does not charge for, and it does nothing about
starvation.

---

### 14. test-and-test-and-set.s

```
.acquire
mov  mutex, %ax     # plain load: is it free?
test $0, %ax
jne .acquire        # no: spin on the load only (no write)
mov  $1, %ax
xchg %ax, mutex     # looks free: now try the atomic swap
test $0, %ax
jne .acquire        # lost the race: start over
```

The lock spins with an ordinary **read** of `mutex` and only tries the atomic
`xchg` when the lock looks free. Plain test-and-set runs `xchg` on every spin
iteration, and every `xchg` is a write to the shared variable.

```
./sweep.sh test-and-test-and-set.s $t 100 "1 5 10 20 50 100"
```

Number of `xchg` executed (the minimum is one per acquire: 200 with 2 threads,
500 with 5):

| -i | 2 thr TAS xchg | 2 thr TTAS xchg | 5 thr TAS xchg | 5 thr TTAS xchg |
|---|---|---|---|---|
| 1 | 400 | 399 | 3317 | 2490 |
| 5 | 399 | 349 | 2868 | 1980 |
| 10 | 396 | 298 | 1564 | 832 |
| 20 | 402 | **200** | 1728 | 623 |
| 50 | 488 | 227 | 3847 | **569** |
| 100 | 548 | 203 | 4144 | **520** |

Total instructions for the same runs:

| -i | 2 thr TAS | 2 thr TTAS | 5 thr TAS | 5 thr TTAS |
|---|---|---|---|---|
| 1 | 3002 | 4198 | 16773 | 25420 |
| 20 | 3010 | 3993 | 10417 | 14682 |
| 50 | 3354 | 4257 | 18893 | 21297 |
| 100 | 3594 | 3714 | 20081 | 13085 |

- **Savings:** atomic operations drop to almost the minimum. With 5 threads
  at `-i 50`, TTAS does 569 `xchg` against 3847 for TAS (6.8x fewer). The few
  extra ones come from a thread that saw the lock free, was preempted before
  its `xchg`, and lost the race.
- **Not a savings in instruction count.** The simulator charges one step per
  instruction, and a waiter spins until its slice ends either way. The
  uncontended path is 3 instructions longer (14 vs 11 per iteration), so TTAS
  usually executes *more* instructions here.
- The real benefit is on multiprocessor hardware. An `xchg` is an atomic
  read-modify-write: it must take the cache line in exclusive state and
  invalidate every other copy, even when it fails. A spinning `mov` hits in
  the spinner's own cache in shared state and creates no bus traffic until the
  holder's release invalidates it. So TTAS cuts cache-coherence traffic and
  leaves the bus free for the lock holder. The `xchg` column is a stand-in for
  that traffic.

---

## Connections to earlier chapters

- **Interrupts and limited direct execution (Ch 6).** The first locking idea
  in the chapter, disabling interrupts, cannot be used by user code.
  `cli`/`sti` are privileged instructions, and LDE only works because the OS
  keeps control through the timer interrupt. A program that could turn off
  interrupts could take over the machine. That is why every lock here relies
  on an atomic instruction instead, and why `yield` is "realistically an OS
  primitive", i.e. a system call that traps into the kernel.
- **The race from Ch 26.** flag.s fails in the same way as the
  `mov count / add / mov count` race in Ch 26: a read-test-write sequence
  that the timer interrupt can split. `xchg` and `fetchadd` work because the
  hardware makes the read and the write a single uninterruptible step, so
  there is no point at which the scheduler can split them.
- **The time slice and the scheduler (Ch 7–8).** `-i` is the round-robin
  quantum. Spin locks waste roughly "the rest of the quantum" each time a
  waiter runs while the holder is preempted, so longer quanta make spinning
  worse (questions 6, 11, 13). `yield` is the cooperative alternative: it gives
  the CPU back to the scheduler early. Under MLFQ, a spinner that burns its
  whole slice gets demoted while the holder may be at a lower priority. If a
  high-priority thread spins on a lock held by a low-priority thread, the
  holder may never run. That is **priority inversion**, the reason the
  chapter moves toward sleeping (queues, park/unpark).
- **Fairness: ticket lock vs lottery/stride (Ch 9).** Test-and-set gives the
  lock to whoever happens to run. The ticket lock serves tickets in strict
  FIFO order, a deterministic round-robin similar to stride scheduling's
  guaranteed fair share, while test-and-set is more like an unweighted
  lottery in which the current runner almost always wins. Question 11 shows
  the cost: fairness forces handoffs to threads that are not running, so
  fairness at the lock level conflicts with the CPU scheduler's decisions.
- **Multiprocessors and caches (Ch 10).** Peterson's algorithm assumes loads
  and stores happen in program order and are seen immediately by the other
  CPU. The simulator guarantees this, but real multiprocessors with store
  buffers and relaxed memory models do not. Thread 0 can read `flag[1] == 0`
  before its own store to `flag[0]` is visible, and both enter. Real code
  needs memory fences, which is why the chapter relies on hardware atomics.
  Test-and-test-and-set exists because of the cache-coherence protocol in
  Ch 10: spinning on a read stays in the local cache, while spinning on
  `xchg` bounces the line between CPUs (question 14).
- **The shared address space (Ch 13).** All of these locks work only because
  threads share one address space. `flag`, `mutex`, `ticket`, `turn` and
  `count` are `.var` globals at the same addresses (for example `flag` at
  100) for every thread. Each thread has its own registers and stack (the
  simulator gives each thread its own `%ax`, `%bx` and stack 1000 bytes
  apart). Two separate *processes* running flag.s would each have a private
  `flag` and would never exclude each other.
