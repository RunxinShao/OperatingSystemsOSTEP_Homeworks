// btree.c -- Q5/Q6: a concurrent B-tree (CLRS-style, keys in every node,
// insert + lookup) with four locking strategies.
//
// usage: ./btree [-m mutex|rwlock|couple|optimistic|partitioned] [-t THREADS]
//                [-o OPS_PER_THREAD] [-p PREFILL] [-r LOOKUP_PERCENT]
//   mutex       Q5: one pthread_mutex_t around every operation
//   rwlock      one pthread_rwlock_t: lookups share it, inserts own it
//   couple      Q6: a reader-writer lock in every node, lock coupling
//               ("crabbing") from the root down; inserts split full nodes
//               on the way down, so they only ever hold parent + child
//   optimistic  Q6: like couple, but an insert read-locks its way down and
//               write-locks only the leaf; if the leaf is full it starts
//               again pessimistically (the "couple" insert)
//   partitioned Q6: NPART independent B-trees, each covering one contiguous
//               slice of the key space and protected by its own mutex
//               (the per-bucket idea of the chapter's hash table, Fig. 29.10,
//               but range-partitioned so the keys stay in sorted order)
// Keys are random in [0, 2^30).  Prints one CSV line:
//   mode,threads,ops,prefill,lookup_pct,seconds,mops
#include "common.h"
#include <sys/resource.h>

#define T     32                   // minimum degree: nodes hold T-1 .. 2T-1 keys
#define MAXK  (2 * T - 1)
#define KEYBITS  30
#define KEYSPACE (1L << KEYBITS)
#define NPARTBITS 6
#define NPART    (1 << NPARTBITS)  // 64 partitions

typedef struct node {
    pthread_rwlock_t lock;
    int n;                         // number of keys
    int leaf;                      // set at creation, never changes
    long key[MAXK];
    struct node *child[MAXK + 1];
} node_t;

typedef struct {
    node_t *root;
    pthread_rwlock_t root_lock;    // protects the root pointer (couple/optimistic)
    pthread_mutex_t mutex;         // mode mutex
    pthread_rwlock_t rw;           // mode rwlock
} __attribute__((aligned(128))) btree_t;   // no false sharing between partitions

static btree_t *trees;            // one tree, or NPART for "partitioned"
static int ntrees = 1;
static int mode;   // 0 mutex, 1 rwlock, 2 couple, 3 optimistic, 4 partitioned
static long ops = 200000, prefill = 1000000;
static int lookup_pct = 90;
static pthread_barrier_t start;

static node_t *new_node(int leaf) {
    node_t *x = calloc(1, sizeof(node_t));
    x->leaf = leaf;
    CHECK(pthread_rwlock_init(&x->lock, NULL));
    return x;
}

// Index of the first key >= k (binary search).
static int find(node_t *x, long k) {
    int lo = 0, hi = x->n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (x->key[mid] < k) lo = mid + 1; else hi = mid;
    }
    return lo;
}

// Split x's full child y = x->child[i] around its median (CLRS B-TREE-
// SPLIT-CHILD).  Caller must own x and y exclusively; the new right half z
// is only reachable through x, so it needs no lock yet.
static void split_child(node_t *x, int i) {
    node_t *y = x->child[i];
    node_t *z = new_node(y->leaf);
    z->n = T - 1;
    memcpy(z->key, y->key + T, (T - 1) * sizeof(long));
    if (!y->leaf)
        memcpy(z->child, y->child + T, T * sizeof(node_t *));
    y->n = T - 1;
    memmove(x->child + i + 2, x->child + i + 1, (x->n - i) * sizeof(node_t *));
    x->child[i + 1] = z;
    memmove(x->key + i + 1, x->key + i, (x->n - i) * sizeof(long));
    x->key[i] = y->key[T - 1];
    x->n++;
}

static void leaf_insert(node_t *x, int i, long k) {
    memmove(x->key + i + 1, x->key + i, (x->n - i) * sizeof(long));
    x->key[i] = k;
    x->n++;
}

// ---- sequential code, used by mutex and rwlock (lock held by caller) ----
static int seq_lookup(btree_t *b, long k) {
    node_t *x = b->root;
    for (;;) {
        int i = find(x, k);
        if (i < x->n && x->key[i] == k) return 1;
        if (x->leaf) return 0;
        x = x->child[i];
    }
}

static int seq_insert(btree_t *b, long k) {
    node_t *x = b->root;
    if (x->n == MAXK) {                       // grow the tree at the top
        node_t *s = new_node(0);
        s->child[0] = x;
        split_child(s, 0);
        b->root = x = s;
    }
    for (;;) {                                // x is never full here
        int i = find(x, k);
        if (i < x->n && x->key[i] == k) return 0;
        if (x->leaf) { leaf_insert(x, i, k); return 1; }
        if (x->child[i]->n == MAXK) {         // split before descending
            split_child(x, i);
            if (k == x->key[i]) return 0;
            if (k > x->key[i]) i++;
        }
        x = x->child[i];
    }
}

// ---- lock coupling ------------------------------------------------------
// Locks are always taken top-down (root_lock, then parent, then child), so
// there is a single global lock order and no deadlock is possible.
static int couple_lookup(btree_t *b, long k) {
    pthread_rwlock_rdlock(&b->root_lock);
    node_t *x = b->root;
    pthread_rwlock_rdlock(&x->lock);
    pthread_rwlock_unlock(&b->root_lock);
    for (;;) {
        int i = find(x, k);
        if ((i < x->n && x->key[i] == k) || x->leaf) {
            int found = i < x->n && x->key[i] == k;
            pthread_rwlock_unlock(&x->lock);
            return found;
        }
        node_t *c = x->child[i];
        pthread_rwlock_rdlock(&c->lock);      // grab the child...
        pthread_rwlock_unlock(&x->lock);      // ...then let go of the parent
        x = c;
    }
}

// Pessimistic insert: write locks all the way down, but never more than two
// at once, because a child that might split was already split on the way.
static int couple_insert(btree_t *b, long k) {
    pthread_rwlock_wrlock(&b->root_lock);
    node_t *x = b->root;
    pthread_rwlock_wrlock(&x->lock);
    if (x->n == MAXK) {
        node_t *s = new_node(0);
        pthread_rwlock_wrlock(&s->lock);
        s->child[0] = x;
        split_child(s, 0);
        b->root = s;
        pthread_rwlock_unlock(&x->lock);
        x = s;
    }
    pthread_rwlock_unlock(&b->root_lock);      // root can't change under us now
    for (;;) {
        int i = find(x, k);
        if (i < x->n && x->key[i] == k) { pthread_rwlock_unlock(&x->lock); return 0; }
        if (x->leaf) {
            leaf_insert(x, i, k);
            pthread_rwlock_unlock(&x->lock);
            return 1;
        }
        node_t *c = x->child[i];
        pthread_rwlock_wrlock(&c->lock);
        if (c->n == MAXK) {
            split_child(x, i);                // x and c are both ours
            if (k == x->key[i]) {
                pthread_rwlock_unlock(&c->lock);
                pthread_rwlock_unlock(&x->lock);
                return 0;
            }
            if (k > x->key[i]) {              // go right, into the new node
                node_t *z = x->child[i + 1];
                pthread_rwlock_wrlock(&z->lock);
                pthread_rwlock_unlock(&c->lock);
                c = z;
            }
        }
        pthread_rwlock_unlock(&x->lock);
        x = c;
    }
}

// Optimistic insert: most inserts only change one leaf, so read-lock the
// path like a lookup and write-lock just the leaf.  A node's leaf flag never
// changes, so we can decide rd/wr before locking the child.
static int optimistic_insert(btree_t *b, long k) {
    pthread_rwlock_rdlock(&b->root_lock);
    node_t *x = b->root;
    if (x->leaf) {                            // tiny tree: be pessimistic
        pthread_rwlock_unlock(&b->root_lock);
        return couple_insert(b, k);
    }
    pthread_rwlock_rdlock(&x->lock);
    pthread_rwlock_unlock(&b->root_lock);
    for (;;) {
        int i = find(x, k);
        if (i < x->n && x->key[i] == k) { pthread_rwlock_unlock(&x->lock); return 0; }
        node_t *c = x->child[i];
        if (c->leaf) pthread_rwlock_wrlock(&c->lock);
        else         pthread_rwlock_rdlock(&c->lock);
        pthread_rwlock_unlock(&x->lock);
        x = c;
        if (x->leaf) break;
    }
    int i = find(x, k);
    if (i < x->n && x->key[i] == k) { pthread_rwlock_unlock(&x->lock); return 0; }
    if (x->n < MAXK) {                        // common case: room in the leaf
        leaf_insert(x, i, k);
        pthread_rwlock_unlock(&x->lock);
        return 1;
    }
    pthread_rwlock_unlock(&x->lock);          // leaf full: a split is needed,
    return couple_insert(b, k);                  // so retry pessimistically
}

static int lookup(btree_t *b, long k) {
    int r;
    switch (mode) {
    case 0: case 4: pthread_mutex_lock(&b->mutex); r = seq_lookup(b, k);
            pthread_mutex_unlock(&b->mutex); return r;
    case 1: pthread_rwlock_rdlock(&b->rw); r = seq_lookup(b, k);
            pthread_rwlock_unlock(&b->rw); return r;
    default: return couple_lookup(b, k);
    }
}

static int insert(btree_t *b, long k) {
    int r;
    switch (mode) {
    case 0: case 4: pthread_mutex_lock(&b->mutex); r = seq_insert(b, k);
            pthread_mutex_unlock(&b->mutex); return r;
    case 1: pthread_rwlock_wrlock(&b->rw); r = seq_insert(b, k);
            pthread_rwlock_unlock(&b->rw); return r;
    case 2: return couple_insert(b, k);
    default: return optimistic_insert(b, k);
    }
}

// Which tree holds key k: the top bits of the key pick a partition.
static inline btree_t *tree_for(long k) {
    return ntrees == 1 ? &trees[0] : &trees[k >> (KEYBITS - NPARTBITS)];
}

static long inserted[1024];

static void *worker(void *arg) {
    int id = (int)(intptr_t)arg;
    uint64_t seed = 0x9E3779B97F4A7C15ULL * (id + 7);
    pthread_barrier_wait(&start);
    for (long i = 0; i < ops; i++) {
        uint64_t r = rng_next(&seed);
        long k = (long)(r % KEYSPACE);
        btree_t *b = tree_for(k);
        if ((int)((r >> 40) % 100) < lookup_pct) lookup(b, k);
        else inserted[id] += insert(b, k);
    }
    return NULL;
}

// Check every B-tree invariant and count keys.
static long check(node_t *x, long lo, long hi, int depth, int *leafdepth) {
    assert(depth == 0 || (x->n >= T - 1 && x->n <= MAXK));
    for (int i = 0; i < x->n; i++) {
        assert(x->key[i] > lo && x->key[i] < hi);
        if (i) assert(x->key[i - 1] < x->key[i]);
    }
    if (x->leaf) {
        if (*leafdepth < 0) *leafdepth = depth;
        assert(*leafdepth == depth);          // all leaves at the same depth
        return x->n;
    }
    long cnt = x->n;
    for (int i = 0; i <= x->n; i++)
        cnt += check(x->child[i], i ? x->key[i - 1] : lo, i < x->n ? x->key[i] : hi,
                     depth + 1, leafdepth);
    return cnt;
}

int main(int argc, char *argv[]) {
    int nthreads = 1, opt;
    const char *names[] = {"mutex", "rwlock", "couple", "optimistic", "partitioned"};
    while ((opt = getopt(argc, argv, "m:t:o:p:r:")) != -1) {
        switch (opt) {
        case 'm': for (mode = 0; mode < 5 && strcmp(optarg, names[mode]); mode++)
                      ;
                  if (mode == 5) { fprintf(stderr, "bad mode\n"); return 1; }
                  break;
        case 't': nthreads = atoi(optarg); break;
        case 'o': ops = atol(optarg); break;
        case 'p': prefill = atol(optarg); break;
        case 'r': lookup_pct = atoi(optarg); break;
        default:  fprintf(stderr, "usage: %s [-m mutex|rwlock|couple|optimistic|partitioned] "
                          "[-t T] [-o OPS] [-p PREFILL] [-r LOOKUP%%]\n", argv[0]);
                  return 1;
        }
    }
    ntrees = mode == 4 ? NPART : 1;
    trees = aligned_alloc(128, ntrees * sizeof(btree_t));
    for (int i = 0; i < ntrees; i++) {
        btree_t *b = &trees[i];
        b->root = new_node(1);
        CHECK(pthread_rwlock_init(&b->root_lock, NULL));
        CHECK(pthread_mutex_init(&b->mutex, NULL));
        CHECK(pthread_rwlock_init(&b->rw, NULL));
    }

    uint64_t seed = 12345;            // same prefill keys in every mode
    long size = 0;
    while (size < prefill) {
        long k = (long)(rng_next(&seed) % KEYSPACE);
        size += seq_insert(tree_for(k), k);
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

    for (int i = 0; i < nthreads; i++) size += inserted[i];
    long cnt = 0;
    int leafdepth = -1;
    for (int i = 0; i < ntrees; i++) {
        long lo = ntrees == 1 ? 0 : (long)i << (KEYBITS - NPARTBITS);
        long hi = ntrees == 1 ? KEYSPACE : (long)(i + 1) << (KEYBITS - NPARTBITS);
        leafdepth = -1;
        cnt += check(trees[i].root, lo - 1, hi, 0, &leafdepth);
    }
    if (cnt != size) { fprintf(stderr, "BUG: tree has %ld keys, expected %ld\n", cnt, size); return 1; }
    struct rusage ru;                  // how often did threads block and sleep?
    getrusage(RUSAGE_SELF, &ru);
    fprintf(stderr, "ok: %ld keys, height %d, voluntary context switches %ld\n",
            cnt, leafdepth + 1, ru.ru_nvcsw);
    printf("%s,%d,%ld,%ld,%d,%.6f,%.4f\n", names[mode], nthreads, ops, prefill,
           lookup_pct, secs, ops * nthreads / secs / 1e6);
    return 0;
}
