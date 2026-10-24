// Q7: compare ways of avoiding demand-zero page faults in the timed loop.
// For each option, allocate 8192 pages, then time one pass of a[i] += 1
// over them (the first timed pass is where any remaining faults land).
//
// usage: ./init-options none|calloc|read|touch|memset|populate
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s none|calloc|read|touch|memset|populate\n", argv[0]);
        return 1;
    }
    const char *how = argv[1];
    long numpages = 8192;
    long pagesize = sysconf(_SC_PAGESIZE);
    long jump = pagesize / sizeof(int);
    volatile long sink = 0;
    int *a;

    double t0 = now_ns();
    if (strcmp(how, "calloc") == 0) {
        a = calloc(numpages, pagesize);                 // zeroed, but still lazy
    } else if (strcmp(how, "populate") == 0) {
#ifdef MAP_POPULATE
        a = mmap(NULL, numpages * pagesize, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);  // kernel pre-faults
        if (a == MAP_FAILED) { perror("mmap"); return 1; }
#else
        fprintf(stderr, "MAP_POPULATE is Linux only\n");
        return 1;
#endif
    } else {
        a = malloc(numpages * pagesize);
        if (strcmp(how, "memset") == 0)                 // write every byte
            memset(a, 0, numpages * pagesize);
        else if (strcmp(how, "touch") == 0)             // write one int per page
            for (long i = 0; i < numpages * jump; i += jump)
                a[i] = 0;
        else if (strcmp(how, "read") == 0)              // read one int per page
            for (long i = 0; i < numpages * jump; i += jump)
                sink += a[i];
        else if (strcmp(how, "none") != 0) {
            fprintf(stderr, "unknown option %s\n", how);
            return 1;
        }
    }
    if (a == NULL) { perror("alloc"); return 1; }
    double t1 = now_ns();
    for (long i = 0; i < numpages * jump; i += jump)
        a[i] += 1;
    double t2 = now_ns();
    for (long i = 0; i < numpages * jump; i += jump)
        sink += a[i];

    printf("%-9s setup %7.2f ms   first timed pass %7.1f ns/page\n",
           how, (t1 - t0) / 1e6, (t2 - t1) / numpages);
    return 0;
}
