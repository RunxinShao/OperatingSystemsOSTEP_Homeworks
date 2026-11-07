#!/bin/bash
# collect.sh -- run every Chapter 29 experiment and save CSVs under data/.
# Each configuration is run TRIALS times; plot.py uses the median.
set -e
cd "$(dirname "$0")"
make -s all
TRIALS=${TRIALS:-5}
D=data
mkdir -p $D

{ echo "date: $(date)"; uname -a; nproc | sed 's/^/cpus: /'
  grep MemTotal /proc/meminfo; gcc --version | head -1; } > $D/machine.txt

echo "== Q1 timer"
./timer > $D/timer.txt

echo "== Q2 precise counter: lock type x threads"
echo "mode,threads,per_thread,seconds,final,expected,trial" > $D/counter.csv
for m in mutex spin atomic none; do
  for t in 1 2 3 4 6 8 12 16 18; do
    for r in $(seq $TRIALS); do echo "$(./counter -m $m -t $t),$r"; done
  done
done >> $D/counter.csv

echo "== Q2 precise counter: number of CPUs (taskset)"
echo "cpus,mode,threads,per_thread,seconds,final,expected,trial" > $D/counter-cpus.csv
for c in 1 2 4 18; do
  for m in mutex spin; do
    for t in 1 2 4 8 16; do
      for r in $(seq $TRIALS); do
        echo "$c,$(taskset -c 0-$((c-1)) ./counter -m $m -t $t),$r"
      done
    done
  done
done >> $D/counter-cpus.csv

echo "== Q3 approximate counter: threads (S=1024), padded vs packed"
echo "layout,threads,per_thread,threshold,seconds,final,expected,trial" > $D/approx-threads.csv
for t in 1 2 3 4 6 8 12 16 18; do
  for r in $(seq $TRIALS); do
    echo "$(./approx -t $t -S 1024),$r"
    echo "$(./approx -t $t -S 1024 -u),$r"
  done
done >> $D/approx-threads.csv

echo "== Q3 approximate counter: threshold (4 and 16 threads)"
echo "layout,threads,per_thread,threshold,seconds,final,expected,trial" > $D/approx-threshold.csv
for t in 4 16; do
  for S in 1 2 4 8 16 32 64 128 256 512 1024; do
    for r in $(seq $TRIALS); do echo "$(./approx -t $t -S $S),$r"; done
  done
done >> $D/approx-threshold.csv

echo "== Q4 list: threads (no per-node work)"
echo "mode,threads,ops,keys,lookup_pct,work,seconds,mops,trial" > $D/list-threads.csv
for m in coarse hoh; do
  for t in 1 2 4 8 16; do
    for r in $(seq $TRIALS); do echo "$(./list -m $m -t $t -o 20000 2>/dev/null),$r"; done
  done
done >> $D/list-threads.csv

echo "== Q4 list: per-node work (1, 8 and 16 threads)"
echo "mode,threads,ops,keys,lookup_pct,work,seconds,mops,trial" > $D/list-work.csv
for m in coarse hoh; do
  for w in 0 5 10 20 50 100 200; do
    for t in 1 8 16; do
      for r in $(seq $TRIALS); do
        echo "$(./list -m $m -t $t -o $((2000000 / (w + 20))) -w $w 2>/dev/null),$r"
      done
    done
  done
done >> $D/list-work.csv

echo "== Q5/Q6 B-tree: strategy x threads x read ratio"
echo "mode,threads,ops,prefill,lookup_pct,seconds,mops,trial" > $D/btree.csv
for rp in 100 90 50; do
  for m in mutex rwlock couple optimistic partitioned; do
    for t in 1 2 4 8 16; do
      for r in $(seq $TRIALS); do
        echo "$(./btree -m $m -t $t -o 300000 -r $rp 2>/dev/null),$r"
      done
    done
  done
done >> $D/btree.csv
echo done
