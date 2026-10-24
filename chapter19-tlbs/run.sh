#!/bin/bash
# Q3: run tlb for 1, 2, 4, ... MAXPAGES pages. Prints "pages ns-per-access".
# usage: ./run.sh MAXPAGES ACCESSES [tlb flags...]
# ACCESSES is the total number of page touches per run; trials = ACCESSES / pages
max=$1
accesses=$2
shift 2
p=1
while [ $p -le $max ]; do
    ./tlb "$@" $p $(( (accesses + p - 1) / p ))
    p=$(( p * 2 ))
done
