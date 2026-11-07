#!/usr/bin/env bash
# Sweep helper for OSTEP Ch28 (Locks) homework.
# Usage: [PC=1000] ./sweep.sh PROG THREADS BX "I_LIST" [extra x86.py args...]
#   Runs PROG with THREADS threads (each with bx=BX) once per interrupt
#   interval in I_LIST and prints one Markdown table row per run:
#     count    = final value of the shared counter (expected = THREADS*BX)
#     total    = instructions executed (from -S)
#     ideal    = instructions for the same work done by ONE thread with
#                bx=THREADS*BX, plus the extra halts (no contention at all)
#     overhead = total - ideal (instructions burned spinning / retrying)
#     xchg     = xchg instructions executed (atomic RMWs = coherence traffic)
#     PC       = executions of instruction address $PC (default 1000)
# run from the directory holding x86.py: this one, else one directory up
cd "$(dirname "$0")" || exit 1
[ -f x86.py ] || cd .. || exit 1
prog=$1; t=$2; bx=$3; ilist=$4; shift 4
pc=${PC:-1000}
args=$(seq 1 "$t" | sed "s/.*/bx=$bx/" | paste -sd, -)
expected=$(( t * bx ))
ideal=$(python3 x86.py -p "$prog" -t 1 -a bx=$(( t * bx )) -c -S "$@" \
        | awk '/STATS:: Instructions/ {print $3}')
ideal=$(( ideal + t - 1 ))
echo "| -i | count | expected | total | ideal | overhead | xchg | PC $pc |"
echo "|---|---|---|---|---|---|---|---|"
for i in $ilist; do
  out=$(python3 x86.py -p "$prog" -t "$t" -a "$args" -i "$i" -M count -c -S "$@")
  total=$(awk '/STATS:: Instructions/ {print $3}' <<<"$out")
  count=$(grep -E '^ *[0-9]+ +[0-9]+ +[0-9]{4} ' <<<"$out" | tail -1 | awk '{print $2}')
  xchg=$(grep -cE '[0-9]{4} xchg' <<<"$out")
  npc=$(grep -cE " $pc [a-z]" <<<"$out")
  echo "| $i | $count | $expected | $total | $ideal | $((total-ideal)) | $xchg | $npc |"
done
