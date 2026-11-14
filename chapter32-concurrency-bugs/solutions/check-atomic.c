// Question 9 test: is a whole vector_add() atomic?
// All elements of v[0] start equal (0) and all elements of v[1] start
// equal (1). If every vector_add() runs as one indivisible step, each
// element sees the same sequence of adds, so every vector stays uniform
// (all 100 elements equal). A non-uniform vector at the end proves that
// two vector_add() calls interleaved element by element.
// Reuses worker() from main-common.c; the upstream main() is renamed.
#define main upstream_main
#include VARIANT
#undef main

int main(int argc, char *argv[]) {
    num_threads = argc > 1 ? atoi(argv[1]) : 2;
    loops = argc > 2 ? atoi(argv[2]) : 20;
    int trials = argc > 3 ? atoi(argv[3]) : 1000;
    int bad = 0, t, i, k;
    pthread_t pid[MAX_THREADS];
    thread_arg_t args[MAX_THREADS];
    for (t = 0; t < trials; t++) {
        for (i = 0; i < 2; i++) vector_init(&v[i], i);
        for (i = 0; i < num_threads; i++) {
            args[i].tid = i;
            args[i].vector_0 = 0;
            args[i].vector_1 = 1;
            args[i].vector_add_order = i % 2;   // same as -d
            Pthread_create(&pid[i], NULL, worker, &args[i]);
        }
        for (i = 0; i < num_threads; i++) Pthread_join(pid[i], NULL);
        int uniform = 1;
        for (i = 0; i < 2; i++)
            for (k = 1; k < VECTOR_SIZE; k++)
                if (v[i].values[k] != v[i].values[0]) uniform = 0;
        if (!uniform) {
            if (bad == 0)
                printf("first non-uniform trial %d: v0[0]=%d v0[99]=%d\n",
                       t, v[0].values[0], v[0].values[99]);
            bad++;
        }
    }
    printf("non-uniform results: %d/%d (n=%d, l=%d)\n",
           bad, trials, num_threads, loops);
    return 0;
}
