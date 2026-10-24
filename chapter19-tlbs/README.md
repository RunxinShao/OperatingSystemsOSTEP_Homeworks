# Chapter 19 — Measuring the TLB

| file | purpose |
|---|---|
| `timer.c` | Q1: timer precision |
| `tlb.c` | Q2, Q5–Q7: measurement program (Linux and macOS) |
| `run.sh` | Q3: sweep 1, 2, 4, ... pages |
| `collect.sh` | run every measurement on one machine into `data/LABEL/` |
| `plot.py` | Q4: graphs (`data/LABEL/tlb.png`, `tlb-machines.png`) |
| `init-options.c` | Q7: ways of initializing the array |

Machines measured (each directory has a `machine.txt` describing it):

- `data/vm-orbstack-linux/` — Linux VM (OrbStack) on an Apple M5 Max, 4 KB pages, pinned to CPU 1 with `sched_setaffinity`
- `data/mac-m5max-macos/` — the same Mac running macOS natively, 16 KB pages; macOS cannot pin threads, so `tlb.c` uses `QOS_CLASS_USER_INTERACTIVE`

```sh
make                       # macOS: cc -O2 -o X X.c for tlb, timer, init-options
./collect.sh LABEL [CPU]   # about 10 minutes
python3 plot.py
```
