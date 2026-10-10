#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <megabytes> [seconds]\n", argv[0]);
        exit(1);
    }
    long mb = atol(argv[1]);
    int secs = (argc > 2) ? atoi(argv[2]) : -1;   // -1 :unlimited runtime at while loop below

    size_t bytes = (size_t) mb * 1024 * 1024;
    size_t n = bytes / sizeof(int);

    // volatile : prevent compiler from optimizing away "useless" write operations
    volatile int *arr = malloc(bytes);
    if (arr == NULL) {
        perror("malloc");
        exit(1);
    }

    printf("pid %d: using %ld MB\n", getpid(), mb);
    fflush(stdout);

    time_t start = time(NULL);
    while (secs < 0 || time(NULL) - start < secs) {
        for (size_t i = 0; i < n; i++) {
            arr[i] += 1;          // touch 每一个元素
        }
    }

    free((void *) arr);
    return 0;
}