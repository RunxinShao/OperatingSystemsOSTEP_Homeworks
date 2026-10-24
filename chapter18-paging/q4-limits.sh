#!/bin/bash
# Chapter 18, question 4: find where paging-linear-translate.py stops working.
# Runs each edge case and prints the command, the simulator's last lines of
# output (or its error) and its exit status.
#
# usage: ./q4-limits.sh [path/to/paging-linear-translate.py] > q4-limits-output.txt

# default: the simulator next to this script, else one directory up
here=$(dirname "$0")
if [ -f "$here/paging-linear-translate.py" ]; then def=$here/paging-linear-translate.py
else def=$here/../paging-linear-translate.py; fi
SIM=${1:-$def}

run() {
    local why=$1; shift
    echo "### $why"
    echo "\$ python3 paging-linear-translate.py $* -c"
    python3 "$SIM" "$@" -c 2>&1 | grep -vE '^ARG|^$' | tail -n 3
    echo "(exit status ${PIPESTATUS[0]})"
    echo
}

echo "== 1. address space vs physical memory"
run "address space bigger than physical memory"   -a 64k -p 32k
run "address space equal to physical memory"      -a 32k -p 32k
run "physical memory just one page bigger"        -P 1k -a 16k -p 17k -u 100 -n 2

echo "== 2. size limits"
run "physical memory of 1 GB is rejected"         -P 1m -a 512m -p 1g
run "largest accepted sizes (1023m)"              -P 1m -a 512m -p 1023m -n 2
run "zero address space"                          -a 0
run "negative address space"                      -a -16k
run "physical memory of 1 byte"                   -p 1

echo "== 3. page size"
run "address space not a power of 2"              -P 1k -a 17k -p 32k
run "page size bigger than address space"         -P 32k -a 16k -p 64k
run "page size equal to address space (1 page)"   -P 16k -a 16k -p 32k -n 2
run "page size not a power of 2"                  -P 3k -a 12k -p 32k
run "physical memory not a multiple of page size" -a 16k -p 30k
run "1-byte pages (works, offset is 0 bits)"      -P 1 -a 16 -p 32 -n 2
run "page size 0 (crashes)"                       -P 0 -a 16k

echo "== 4. -u (percent of pages used)"
run "-u above 100 acts like 100"                  -u 101 -n 2
run "-u below 0 acts like 0"                      -u -1 -n 2

echo "== 5. -A (explicit addresses)"
run "last valid address of a 16k space"           -a 16k -A 16383
run "address just past the end (crashes)"         -a 16k -A 16384
run "address that is not a number (crashes)"      -A 100,abc
