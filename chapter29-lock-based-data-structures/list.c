// list.c -- Q4: a sorted linked list, with one big lock (Fig. 29.8 style)
// or with hand-over-hand locking (lock coupling).
//
// usage: ./list [-m coarse|hoh] [-t THREADS] [-o OPS_PER_THREAD] [-k KEYS]
//               [-r LOOKUP_PERCENT] [-w WORK_PER_NODE]
//   The list is prefilled with KEYS/2 keys drawn from [0, KEYS).  Each op
//   picks a random key: LOOKUP_PERCENT% are lookups; the rest are split
//   evenly between insert and delete, so the length stays about KEYS/2.
//   WORK_PER_NODE adds a busy loop at every node visited, standing in for an
//   expensive comparison (e.g. strcmp on long keys).
// Prints one CSV line: mode,threads,ops,keys,lookup_pct,work,seconds,mops
#include "common.h"

typedef struct node {
    long key;
    struct node *next;
    pthread_mutex_t lock;          // used only by hand-over-hand
} node_t;

typedef struct {
    node_t head;                   // sentinel: key = -1, never removed
    pthread_mutex_t lock;          // used only by coarse
} list_t;

static list_t L;
static int hoh, work;
static long ops = 100000, keys = 1000;
static int lookup_pct = 80;
static pthread_barrier_t start;

static inline void visit(void) {               // simulated per-node work
    for (volatile int i = 0; i < work; i++)
        ;
}

static node_t *new_node(long key) {
    node_t *n = malloc(sizeof(node_t));
    n->key = key;
    n->next = NULL;
    CHECK(pthread_mutex_init(&n->lock, NULL));
    return n;
}

// ---- coarse-grained: one lock around the whole traversal ----------------
// op: 0 lookup, 1 insert, 2 delete.  Returns 1 on success.
static int coarse_op(int op, long key) {
    int ret = 0;
    pthread_mutex_lock(&L.lock);
    node_t *prev = &L.head, *cur = prev->next;
    while (cur && cur->key < key) {
        visit();
        prev = cur;
        cur = cur->next;
    }
    int found = cur && cur->key == key;
    if (op == 0) {
        ret = found;
    } else if (op == 1 && !found) {
        node_t *n = new_node(key);
        n->next = cur;
        prev->next = n;
        ret = 1;
    } else if (op == 2 && found) {
        prev->next = cur->next;
        free(cur);
        ret = 1;
    }
    pthread_mutex_unlock(&L.lock);
    return ret;
}

// ---- hand-over-hand: hold at most two node locks, always prev then cur ---
// A thread can only reach a node by first locking its predecessor, so while
// we hold prev nobody else can be holding or waiting for cur: that is what
// makes it safe to unlink and free cur.
static int hoh_op(int op, long key) {
    int ret = 0;
    node_t *prev = &L.head;
    pthread_mutex_lock(&prev->lock);
    node_t *cur = prev->next;
    if (cur) pthread_mutex_lock(&cur->lock);
    while (cur && cur->key < key) {
        visit();
        pthread_mutex_unlock(&prev->lock);       // release the one behind
        prev = cur;
        cur = cur->next;
        if (cur) pthread_mutex_lock(&cur->lock); // grab the one ahead
    }
    int found = cur && cur->key == key;
    if (op == 0) {
        ret = found;
    } else if (op == 1 && !found) {
        node_t *n = new_node(key);
        n->next = cur;
        prev->next = n;
        ret = 1;
    } else if (op == 2 && found) {
        prev->next = cur->next;
        pthread_mutex_unlock(&cur->lock);
        pthread_mutex_destroy(&cur->lock);
        free(cur);
        cur = NULL;
        ret = 1;
    }
    if (cur) pthread_mutex_unlock(&cur->lock);
    pthread_mutex_unlock(&prev->lock);
    return ret;
}

static void *worker(void *arg) {
    uint64_t seed = 0x9E3779B97F4A7C15ULL * ((intptr_t)arg + 1);
    pthread_barrier_wait(&start);
    for (long i = 0; i < ops; i++) {
        uint64_t r = rng_next(&seed);
        long key = (long)(r % keys);
        int pick = (int)((r >> 32) % 100);
        int op = pick < lookup_pct ? 0 : (pick - lookup_pct) % 2 ? 1 : 2;
        if (hoh) hoh_op(op, key); else coarse_op(op, key);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int nthreads = 1, opt;
    while ((opt = getopt(argc, argv, "m:t:o:k:r:w:")) != -1) {
        switch (opt) {
        case 'm': hoh = !strcmp(optarg, "hoh"); break;
        case 't': nthreads = atoi(optarg); break;
        case 'o': ops = atol(optarg); break;
        case 'k': keys = atol(optarg); break;
        case 'r': lookup_pct = atoi(optarg); break;
        case 'w': work = atoi(optarg); break;
        default:  fprintf(stderr, "usage: %s [-m coarse|hoh] [-t T] [-o OPS] "
                          "[-k KEYS] [-r LOOKUP%%] [-w WORK]\n", argv[0]); return 1;
        }
    }
    L.head.key = -1;
    CHECK(pthread_mutex_init(&L.head.lock, NULL));
    CHECK(pthread_mutex_init(&L.lock, NULL));
    // Prefill with the even keys (sorted order, built back to front).
    for (long k = (keys - 1) / 2 * 2; k >= 0; k -= 2) {
        node_t *n = new_node(k);
        n->next = L.head.next;
        L.head.next = n;
    }
    CHECK(pthread_barrier_init(&start, NULL, nthreads + 1));
    pthread_t tid[1024];
    for (int i = 0; i < nthreads; i++)
        CHECK(pthread_create(&tid[i], NULL, worker, (void *)(intptr_t)i));
    pthread_barrier_wait(&start);
    double t0 = now_sec();
    for (int i = 0; i < nthreads; i++)
        CHECK(pthread_join(tid[i], NULL));
    double secs = now_sec() - t0;

    // Check the list is still sorted with no duplicates.
    long len = 0;
    for (node_t *n = L.head.next; n; n = n->next, len++)
        if (n->next) assert(n->key < n->next->key);
    fprintf(stderr, "final length %ld\n", len);
    printf("%s,%d,%ld,%ld,%d,%d,%.6f,%.4f\n", hoh ? "hoh" : "coarse", nthreads,
           ops, keys, lookup_pct, work, secs, ops * nthreads / secs / 1e6);
    return 0;
}
