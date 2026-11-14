#!/usr/bin/env bash
# Experiment driver for OSTEP Ch32 (threads-bugs) homework.
# Usage: ./run.sh [q1|q2|q3|q4|q5|q6|q7|q8|q9|q10|knee|final|all]   (run from anywhere)
# Raw outputs go to data/*.txt. Timings are the program's own -t output;
# every timing cell is the median of $REPS runs (default 5); a run that
# exceeds $TLIM seconds (default 60) is reported as ">TLIM".
# Deadlock detection: a run killed by `timeout $TMO` (default 1 s,
# exit 124) counts as hung.
cd "$(dirname "$0")" || exit 1
make -s all >/dev/null || exit 1
mkdir -p data
REPS=${REPS:-5}
TMO=${TMO:-1}
TLIM=${TLIM:-60}

median() { sort -g | awk '{a[NR]=$1} END {print a[int((NR+1)/2)]}'; }

# timing PROG ARGS... -> "median_time median_retries" over REPS runs
timing() {
  local t r out
  t=(); r=()
  for _ in $(seq "$REPS"); do
    out=$(timeout "$TLIM" ./"$@" -t)
    # a run that exceeds TLIM seconds: report ">TLIM" and skip the rest
    if [ $? -eq 124 ]; then echo ">$TLIM -"; return; fi
    t+=("$(awk '/^Time:/ {print $2}' <<<"$out")")
    r+=("$(awk '/^Retries:/ {print $2}' <<<"$out")")
  done
  local mt mr
  mt=$(printf '%s\n' "${t[@]}" | median)
  mr=$(printf '%s\n' "${r[@]}" | grep . | median)
  echo "$mt ${mr:--}"
}

# deadfrac PROG RUNS ARGS... -> "hung/RUNS"
deadfrac() {
  local prog=$1 runs=$2 d=0; shift 2
  for _ in $(seq "$runs"); do
    timeout "$TMO" ./"$prog" "$@" >/dev/null
    [ $? -eq 124 ] && d=$((d + 1))
  done
  echo "$d/$runs"
}

q1() {
  echo "# 500 runs of ./vector-deadlock -n 2 -l 1 -v; '|' = newline;"
  echo "# a leading space marks a thread-1 line (indentation squeezed)"
  for _ in $(seq 500); do
    ./vector-deadlock -n 2 -l 1 -v | tr -s ' ' | tr '\n' '|'; echo
  done | sort | uniq -c | sort -rn
}

q2() {
  echo "# vector-deadlock -n 2 -d, 50 runs each, timeout ${TMO}s"
  echo "| -l | hung (with -d) | hung (without -d) |"
  echo "|---|---|---|"
  for l in 1 10 100 1000 10000 100000; do
    echo "| $l | $(deadfrac vector-deadlock 50 -n 2 -l $l -d)" \
         "| $(deadfrac vector-deadlock 50 -n 2 -l $l) |"
  done
}

q3() {
  echo "# vector-deadlock -d, 30 runs per cell, timeout ${TMO}s"
  echo "| -n | -l 1 | -l 10 | -l 100 | -l 1000 |"
  echo "|---|---|---|---|---|"
  for n in 1 2 3 4 8 16 32; do
    row="| $n "
    for l in 1 10 100 1000; do
      row+="| $(deadfrac vector-deadlock 30 -n $n -l $l -d) "
    done
    echo "$row|"
  done
}

q4() {
  echo "# vector_add(&v[0], &v[0]) with each variant, timeout ${TMO}s"
  for x in deadlock global-order; do
    echo "== self-add-$x"
    timeout "$TMO" ./self-add-$x; echo "exit=$?"
  done
}

q9() {
  echo "# check-atomic: does every vector stay uniform? (-d pattern, 1000 trials)"
  for x in global-order try-wait avoid-hold-and-wait nolock; do
    for n in 2 4 8; do
      echo "== check-atomic-$x n=$n"
      ./check-atomic-$x $n 20 1000
    done
  done
}

q5() {
  echo "# vector-global-order -d, median of $REPS (seconds)"
  echo "## loops sweep, -n 2"
  echo "| -l | time |"; echo "|---|---|"
  for l in 100000 200000 400000 800000 1600000; do
    echo "| $l | $(timing vector-global-order -n 2 -l $l -d | cut -d' ' -f1) |"
  done
  echo "## threads sweep, -l 100000"
  echo "| -n | time |"; echo "|---|---|"
  for n in 1 2 4 8 16 32; do
    echo "| $n | $(timing vector-global-order -n $n -l 100000 -d | cut -d' ' -f1) |"
  done
}

q6() {
  echo "# vector-global-order -l 100000 -d, with/without -p, median of $REPS"
  echo "| -n | no -p | -p |"; echo "|---|---|---|"
  for n in 1 2 4 8 16 32; do
    a=$(timing vector-global-order -n $n -l 100000 -d | cut -d' ' -f1)
    b=$(timing vector-global-order -n $n -l 100000 -d -p | cut -d' ' -f1)
    echo "| $n | $a | $b |"
  done
}

q7() {
  echo "# vector-try-wait -l 100000 -d, median of $REPS (time, retries)"
  echo "| -n | time | retries | time -p | retries -p |"
  echo "|---|---|---|---|---|"
  for n in 1 2 4 8 16 32; do
    read -r a ar <<<"$(timing vector-try-wait -n $n -l 100000 -d)"
    read -r b br <<<"$(timing vector-try-wait -n $n -l 100000 -d -p)"
    echo "| $n | $a | $ar | $b | $br |"
  done
}

q8() {
  echo "# vector-avoid-hold-and-wait -l 100000 -d, median of $REPS"
  echo "| -n | no -p | -p |"; echo "|---|---|---|"
  for n in 1 2 4 8 16 32; do
    a=$(timing vector-avoid-hold-and-wait -n $n -l 100000 -d | cut -d' ' -f1)
    b=$(timing vector-avoid-hold-and-wait -n $n -l 100000 -d -p | cut -d' ' -f1)
    echo "| $n | $a | $b |"
  done
}

q10() {
  echo "# vector-nolock -l 100000 -d, median of $REPS"
  echo "| -n | no -p | -p |"; echo "|---|---|---|"
  for n in 1 2 4 8 16 32; do
    a=$(timing vector-nolock -n $n -l 100000 -d | cut -d' ' -f1)
    b=$(timing vector-nolock -n $n -l 100000 -d -p | cut -d' ' -f1)
    echo "| $n | $a | $b |"
  done
}

knee() {
  echo "# -p, -l 1000000, median of $REPS: per-thread work is constant,"
  echo "# so ideal scaling is a flat row"
  echo "| -n | global-order -p | global-order -p -d | nolock -p -d |"
  echo "|---|---|---|---|"
  for n in 1 2 4 6 7 8 12 16; do
    a=$(timing vector-global-order -n $n -l 1000000 -p | cut -d' ' -f1)
    b=$(timing vector-global-order -n $n -l 1000000 -d -p | cut -d' ' -f1)
    c=$(timing vector-nolock -n $n -l 1000000 -d -p | cut -d' ' -f1)
    echo "| $n | $a | $b | $c |"
  done
  echo "# address of v[] (sizeof(vector_t) = 48-byte mutex + 400 = 448)"
  nm vector-global-order | grep ' v$'
}

final() {
  local L=${FINAL_L:-1000000}
  echo "# all variants, -l $L, median of $REPS (seconds), limit ${TLIM}s/run."
  echo "# 'same' = no -p (all threads on v[0],v[1]); 'sep' = -p."
  echo "# -d is passed to every variant except vector-deadlock without -p."
  for mode in same sep; do
    echo "## $mode"
    echo "| variant | -n 2 | -n 4 | -n 8 | -n 16 |"
    echo "|---|---|---|---|---|"
    for v in vector-deadlock vector-global-order vector-try-wait \
             vector-avoid-hold-and-wait vector-nolock; do
      row="| $v "
      for n in 2 4 8 16; do
        flags="-n $n -l $L -d"
        [ "$mode" = sep ] && flags="$flags -p"
        [ "$v" = vector-deadlock ] && [ "$mode" = same ] && flags="-n $n -l $L"
        row+="| $(timing $v $flags | cut -d' ' -f1) "
      done
      echo "$row|"
    done
  done
}

what=${1:-all}
if [ "$what" = all ]; then
  for q in q1 q2 q3 q4 q5 q6 q7 q8 q9 q10 knee final; do
    echo ">> $q" >&2; $q | tee "data/$q.txt"
  done
else
  $what | tee "data/$what.txt"
fi
