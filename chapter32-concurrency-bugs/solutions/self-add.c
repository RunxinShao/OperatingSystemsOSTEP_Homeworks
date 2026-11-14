// Calls vector_add(&v[0], &v[0]) once, using the vector_add() from the
// variant named by VARIANT (e.g. -DVARIANT='"../vector-deadlock.c"').
// The upstream main() is renamed so we can supply our own.
#define main upstream_main
#include VARIANT
#undef main

int main(void) {
    vector_init(&v[0], 1);
    printf("calling vector_add(&v[0], &v[0])...\n");
    fflush(stdout);
    vector_add(&v[0], &v[0]);
    printf("returned; v[0].values[0] = %d\n", v[0].values[0]);
    return 0;
}
