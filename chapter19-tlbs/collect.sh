#!/bin/bash
# Run every chapter 19 measurement on this machine and save it under
# data/LABEL/. Build first with `make` (or `cc -O2 -o tlb tlb.c` etc).
#
# usage: ./collect.sh LABEL [CPU]
#   LABEL  name of the output directory, e.g. vm-orbstack, mac-m5max, khoury
#   CPU    CPU to pin to on Linux (default 1; CPU 0 handles most interrupts)
set -e
label=${1:?usage: $0 LABEL [CPU]}
cpu=${2:-1}
out=data/$label
mkdir -p "$out"

# machine description
{
    echo "label: $label"
    echo "date: $(date)"
    uname -a
    if [ "$(uname)" = Darwin ]; then
        sysctl -n machdep.cpu.brand_string
        echo "cpus: $(sysctl -n hw.ncpu) (perf levels: $(sysctl -n hw.perflevel0.logicalcpu 2>/dev/null) + $(sysctl -n hw.perflevel1.logicalcpu 2>/dev/null))"
        echo "memory: $(( $(sysctl -n hw.memsize) / 1073741824 )) GB"
        echo "pinning: not possible on macOS; tlb.c uses QOS_CLASS_USER_INTERACTIVE"
    else
        lscpu | grep -E 'Architecture|Model name|Vendor ID|^CPU\(s\)'
        grep MemTotal /proc/meminfo
        echo "transparent hugepages: $(cat /sys/kernel/mm/transparent_hugepage/enabled 2>/dev/null)"
        echo "pinning: sched_setaffinity to CPU $cpu"
    fi
    echo "page size: $(getconf PAGESIZE)"
    cc --version 2>&1 | head -1
} > "$out/machine.txt"
cat "$out/machine.txt"

./timer > "$out/timer.txt"

# Q3/Q4: three sweeps of each access mode
for r in 1 2 3; do
    echo "sweep $r" >&2
    ./run.sh 32768 100000000 -c $cpu       > "$out/stride-$r.dat" 2>/dev/null
    ./run.sh 32768 100000000 -c $cpu -s    > "$out/spread-$r.dat" 2>/dev/null
    ./run.sh 32768 30000000  -c $cpu -s -d > "$out/chase-$r.dat"  2>/dev/null
done

# finer steps around the TLB sizes
for p in $(seq 64 16 288) $(seq 1024 256 4096); do
    ./tlb -c $cpu -s -d $p $(( 30000000 / p )) 2>/dev/null
done > "$out/chase-fine.dat"

# Q3: run-to-run spread vs number of accesses (5 runs each)
{
    echo "# pages total_accesses trials min_ns max_ns   (5 runs each, ./tlb -s -d)"
    for p in 16 512 8192; do
        for acc in 1000 10000 100000 1000000 10000000 100000000; do
            t=$(( (acc + p - 1) / p ))
            vals=$(for i in 1 2 3 4 5; do ./tlb -c $cpu -s -d $p $t 2>/dev/null | awk '{print $2}'; done | sort -g)
            echo "$p $acc $t $(echo "$vals" | head -1) $(echo "$vals" | tail -1)"
        done
    done
} > "$out/trials.txt"

# Q6: pinned vs not pinned (8 runs each, 64 / 512 / 8192 pages)
{
    echo "# pages cpu : ns per access for 8 runs   (./tlb -s -d, 10^7 accesses; cpu -1 = not pinned)"
    for p in 64 512 8192; do
        for c in 0 $cpu -1; do
            echo "$p cpu=$c : $(for i in 1 2 3 4 5 6 7 8; do ./tlb -c $c -s -d $p $(( 10000000 / p )) 2>/dev/null | awk '{print $2}'; done | tr '\n' ' ')"
        done
    done
} > "$out/pinning.txt"

# Q7: with and without initializing the array first
{
    echo "# pages trials init_ns noinit_ns   (default mode)"
    for p in 1024 8192 32768; do
        for t in 1 10 1000; do
            echo "$p $t $(./tlb -c $cpu $p $t 2>/dev/null | awk '{print $2}') $(./tlb -c $cpu -n $p $t 2>/dev/null | awk '{print $2}')"
        done
    done
} > "$out/init.txt"

# Q7: ways of avoiding the page faults (3 runs each)
for how in none calloc read touch memset populate; do
    for i in 1 2 3; do ./init-options $how 2>&1; done
done > "$out/init-options.txt"

echo "done: $out" >&2
