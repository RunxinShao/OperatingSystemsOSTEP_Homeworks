// Q2: measure the average cost of touching one int on each of NUMPAGES pages.
//
// usage: ./tlb [-c CPU] [-s] [-d] [-n] NUMPAGES TRIALS
//   -c CPU  pin to this CPU (default 0; -1 = don't pin)            (Q6)
//   -s      spread: touch a pseudo-random cache line on each page, so the
//           pages don't all land in the same few cache sets
//   -d      dependent: each page holds a pointer to the next page and the
//           loop follows it, so misses can't overlap
//   -n      no init: skip touching the array before the timed loop (Q7)
// Prints "NUMPAGES ns-per-access".
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

int main(int argc, char *argv[]) {
    int cpu = 0, spread = 0, dependent = 0, init = 1, opt;
    while ((opt = getopt(argc, argv, "c:sdn")) != -1) {
        switch (opt) {
        case 'c': cpu = atoi(optarg); break;
        case 's': spread = 1; break;
        case 'd': dependent = 1; break;
        case 'n': init = 0; break;
        default:
            fprintf(stderr, "usage: %s [-c CPU] [-s] [-d] [-n] NUMPAGES TRIALS\n", argv[0]);
            return 1;
        }
    }
    if (argc - optind < 2) {
        fprintf(stderr, "usage: %s [-c CPU] [-s] [-d] [-n] NUMPAGES TRIALS\n", argv[0]);
        return 1;
    }
    long numpages = atol(argv[optind]);
    long trials   = atol(argv[optind + 1]);

    // Q6: keep the whole run on one CPU, and hence one TLB
    if (cpu >= 0) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu, &set);
        if (sched_setaffinity(0, sizeof(set), &set) != 0) {
            perror("sched_setaffinity");
            return 1;
        }
    }

    long pagesize = sysconf(_SC_PAGESIZE);
    long jump = pagesize / sizeof(int);
    int *a = aligned_alloc(pagesize, numpages * pagesize);
    if (a == NULL) {
        perror("aligned_alloc");
        return 1;
    }
    // Q7: touch every page once first so demand-zero page faults
    // are paid here and not inside the timed loop
    if (init)
        memset(a, 0, numpages * pagesize);

    // index of the int touched on page p: offset 0, or with -s the start
    // of a cache line picked by hashing p
    long lines = pagesize / 64;
    #define LINE(p) ((((unsigned long)(p) * 2654435761u) >> 16) % lines)
    #define SLOT(p) ((p) * jump + (spread ? LINE(p) * (64 / sizeof(int)) : 0))

    double start, end;
    long check;
    if (!dependent) {
        start = now_ns();
        for (long t = 0; t < trials; t++)
            for (long p = 0; p < numpages; p++)
                a[SLOT(p)] += 1;
        end = now_ns();

        // Q5: use the results so the compiler can't drop the loop
        check = 0;
        for (long p = 0; p < numpages; p++)
            check += a[SLOT(p)];
        check -= numpages * trials;
    } else {
        // page p points at page p+1, the last page back at page 0
        for (long p = 0; p < numpages; p++)
            *(int **)&a[SLOT(p)] = &a[SLOT((p + 1) % numpages)];
        int *q = &a[SLOT(0)];
        start = now_ns();
        for (long i = 0; i < trials * numpages; i++)
            q = *(int **)q;
        end = now_ns();
        check = q != &a[SLOT(0)];
    }
    if (check != 0) {
        fprintf(stderr, "check failed\n");
        return 1;
    }

    printf("%ld %.3f\n", numpages, (end - start) / (numpages * trials));
    free(a);
    return 0;
}
