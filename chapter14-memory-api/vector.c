#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *data;
    size_t size;
    size_t cap;
} vector;

void vec_init(vector *v) {
    v->data = NULL;
    v->size = 0;
    v->cap = 0;
}

void vec_push(vector *v, int x) {
    if (v->size == v->cap) {
        size_t newcap = v->cap == 0 ? 1 : v->cap * 2;   // grow by doubling
        int *p = realloc(v->data, newcap * sizeof(int));
        if (p == NULL) {
            perror("realloc");
            exit(1);
        }
        v->data = p;
        v->cap = newcap;
    }
    v->data[v->size++] = x;
}

int vec_get(vector *v, size_t i) {
    return v->data[i];
}

void vec_free(vector *v) {
    free(v->data);
    vec_init(v);
}

int main(int argc, char *argv[]) {
    int n = (argc > 1) ? atoi(argv[1]) : 10;
    vector v;
    vec_init(&v);
    for (int i = 0; i < n; i++)
        vec_push(&v, i * i);

    long sum = 0;
    for (size_t i = 0; i < v.size; i++)
        sum += vec_get(&v, i);
    printf("size %zu, cap %zu, sum %ld\n", v.size, v.cap, sum);

    vec_free(&v);
    return 0;
}
