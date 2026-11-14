#!/bin/bash
# test.sh -- run every Chapter 31 solution, check the ordering it must
# guarantee, and save the outputs under data/.
cd "$(dirname "$0")"
make -s all || exit 1
mkdir -p data
fail=0
check() { if eval "$2"; then echo "PASS  $1"; else echo "FAIL  $1"; fail=1; fi; }

# Q1: "child" must come before "parent: end" (child sleeps 1 s first)
./fork-join > data/fork-join.txt
check "fork-join: parent waits for the sleeping child" \
      '[ "$(sed -n 2p data/fork-join.txt)" = child ]'

# Q2: both "before" lines precede both "after" lines, in 3 timing variants
{ for v in "" 1 2; do echo "\$ ./rendezvous $v"; ./rendezvous $v; done; } > data/rendezvous.txt
ok=1
for i in $(seq 500); do
  for v in "" 1 2; do
    [ -n "$v" ] && [ $i -gt 3 ] && continue        # sleeps: only 3 runs each
    ./rendezvous $v | awk '/after/{a=1} /before/&&a{exit 1}' || ok=0
  done
done
check "rendezvous: no 'after' before both 'before's (500 runs + sleeps)" '[ $ok = 1 ]'

# Q3: every "before" precedes every "after", for 1..32 threads
ok=1
for n in 1 2 3 4 8 16 32; do
  for i in $(seq 200); do
    ./barrier $n | awk '/after/{a=1} /before/&&a{exit 1}' || ok=0
  done
done
check "barrier: 1..32 threads x 200 runs" '[ $ok = 1 ]'
{ echo '$ ./barrier 4'; ./barrier 4
  echo '$ LATE=1 ./barrier 4      (first arrival sleeps 1 s)'
  LATE=1 stdbuf -oL ./barrier 4 | python3 -c '
import sys, time
t0 = time.time()
for line in sys.stdin:
    print(f"{time.time() - t0:5.2f} s  {line}", end="")'
} > data/barrier.txt
ok=1; LATE=1 ./barrier 8 | awk '/after/{a=1} /before/&&a{exit 1}' || ok=0
check "barrier: late thread holds everyone back" '[ $ok = 1 ]'

# Q4/Q5: readers vs writers
for p in reader-writer reader-writer-plain-turnstile reader-writer-nostarve; do
  { echo "\$ READ_US=10000 WRITE_US=10000 ./$p 3 2 5"
    READ_US=10000 WRITE_US=10000 ./$p 3 2 5 2>&1 | paste -sd' ' | fold -s -w 78
    for i in 1 2 3; do
      echo "\$ READ_US=1000 ./$p 8 2 200 > /dev/null   (run $i)"
      READ_US=1000 ./$p 8 2 200 2>&1 >/dev/null
    done
  } > data/$p.txt
  out=$(./$p 4 4 2000 2>/dev/null | tail -1)
  check "$p: value = writers x loops (16 threads, no sleeps)" '[ "$out" = "end: value 8000" ]'
done

# Q6: no-starve mutex vs plain semaphore
{ for n in 2 4 8 16; do
    for m in ns sem; do
      echo "\$ ./mutex-nostarve $n 1 $m"; ./mutex-nostarve $n 1 $m | grep -v parent
    done
  done
} > data/mutex-nostarve.txt
ok=1
for n in 2 4 8 16; do
  w=$(./mutex-nostarve $n 1 ns | awk '/summary/{print $NF}')
  [ "$w" -le $((2 * (n - 1))) ] || ok=0
done
check "mutex-nostarve: worst overtaken <= 2(N-1) for N = 2..16" '[ $ok = 1 ]'
grep -q BROKEN data/mutex-nostarve.txt && fail=1
exit $fail
