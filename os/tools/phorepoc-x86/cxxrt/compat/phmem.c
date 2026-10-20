/* phmem.c -- OURS (Phorepoc libphcompat). The allocator behind WebKit's FastMalloc (bmalloc's system-malloc mode is patched to call
   these): Open C's malloc is an RHeap with no aligned allocation and a free() that cannot take interior pointers, while JSC needs
   16-byte-and-larger aligned blocks that are later free()d (MarkedBlock = 16 KB aligned).
     - every block gets an 8-byte header {raw pointer, size} in front of a 16-byte aligned pointer (small + any alignment);
     - alignments >= 4096 with size <= alignment come from "pools": a chunk of PH_POOL_BLOCKS aligned blocks carved from one big
       malloc (no 2x over-allocation per block). Freed blocks go on one free list per block size and are reused; pool memory is
       never returned to the heap. The pool table grows on demand and is kept sorted by address, so free() finds a block's pool
       by binary search. (Until 2026-10-14 the table was a fixed 32 pools x 32 blocks = 16 MB of MarkedBlocks: past that JSC
       failed with MemoryExhaustion while the heap had hundreds of MB left -- WPT dom/nodes/name-validation.html.)
   One mutex guards the pools. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

typedef struct { void* raw; size_t size; } ph_hdr;

static void* alloc_hdr(size_t align, size_t size)
{
    char* raw; uintptr_t p; ph_hdr* h; size_t total;
    if (align < 16) align = 16;
    total = size + align + sizeof(ph_hdr);
    if (total < size) return NULL;
    raw = (char*)malloc(total);
    if (!raw) return NULL;
    p = ((uintptr_t)raw + sizeof(ph_hdr) + align - 1) & ~(uintptr_t)(align - 1);
    h = (ph_hdr*)p - 1; h->raw = raw; h->size = size;
    return (void*)p;
}

#define PH_POOL_BLOCKS 64   /* blocks per pool: 1 MB of 16 KB MarkedBlocks */
#define PH_SIZE_CLASSES 8   /* distinct pooled block sizes (JSC uses 16 KB; a few page-size users) */
typedef struct { char* base; size_t bsize; } ph_pool;
typedef struct { size_t bsize; void* freelist; char* bump; char* end; } ph_class;
static ph_pool* g_pools;     /* sorted by base */
static int g_npools, g_cap;
static ph_class g_classes[PH_SIZE_CLASSES];
static int g_nclasses;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* the pool holding p, or NULL (caller holds g_lock) */
static ph_pool* pool_of(const void* p)
{
    int lo = 0, hi = g_npools - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        ph_pool* pl = &g_pools[mid];
        if ((const char*)p < pl->base) hi = mid - 1;
        else if ((const char*)p >= pl->base + PH_POOL_BLOCKS * pl->bsize) lo = mid + 1;
        else return pl;
    }
    return NULL;
}

static int add_pool(char* base, size_t bsize)
{
    int i;
    if (g_npools == g_cap) {
        int cap = g_cap ? g_cap * 2 : 64;
        ph_pool* n = (ph_pool*)realloc(g_pools, cap * sizeof(ph_pool));
        if (!n) return 0;
        g_pools = n; g_cap = cap;
    }
    for (i = g_npools; i > 0 && g_pools[i - 1].base > base; --i)
        g_pools[i] = g_pools[i - 1];
    g_pools[i].base = base; g_pools[i].bsize = bsize;
    g_npools++;
    return 1;
}

static void* pool_alloc(size_t bsize)
{
    int i; void* r = NULL; ph_class* c = NULL;
    pthread_mutex_lock(&g_lock);
    for (i = 0; i < g_nclasses; ++i)
        if (g_classes[i].bsize == bsize) { c = &g_classes[i]; break; }
    if (!c && g_nclasses < PH_SIZE_CLASSES) {
        c = &g_classes[g_nclasses++];
        c->bsize = bsize; c->freelist = NULL; c->bump = c->end = NULL;
    }
    if (c) {
        if (c->freelist) { r = c->freelist; c->freelist = *(void**)r; }
        else {
            if (c->bump == c->end) {
                char* raw = (char*)malloc(bsize * (PH_POOL_BLOCKS + 1));
                if (raw) {
                    char* base = (char*)(((uintptr_t)raw + bsize - 1) & ~(uintptr_t)(bsize - 1));
                    if (add_pool(base, bsize)) { c->bump = base; c->end = base + PH_POOL_BLOCKS * bsize; }
                    else free(raw);
                }
            }
            if (c->bump != c->end) { r = c->bump; c->bump += bsize; }
        }
    }
    pthread_mutex_unlock(&g_lock);
    return r;
}

static int pool_free(void* p)
{
    int hit = 0; ph_pool* pl;
    pthread_mutex_lock(&g_lock);
    pl = pool_of(p);
    if (pl) {
        int i;
        for (i = 0; i < g_nclasses; ++i)
            if (g_classes[i].bsize == pl->bsize) { *(void**)p = g_classes[i].freelist; g_classes[i].freelist = p; break; }
        hit = 1;
    }
    pthread_mutex_unlock(&g_lock);
    return hit;
}

static size_t pool_size_of(const void* p)
{
    size_t r = 0; ph_pool* pl;
    pthread_mutex_lock(&g_lock);
    pl = pool_of(p);
    if (pl) r = pl->bsize;
    pthread_mutex_unlock(&g_lock);
    return r;
}

void* ph_memalign(size_t align, size_t size)
{
    if (align >= 4096 && size <= align && (align & (align - 1)) == 0) {
        void* r = pool_alloc(align);
        if (r) return r;
    }
    return alloc_hdr(align, size);
}
void* ph_malloc(size_t size) { return alloc_hdr(16, size); }
void* ph_calloc(size_t n, size_t m)
{
    size_t t = n * m; void* p;
    if (n && t / n != m) return NULL;
    p = alloc_hdr(16, t);
    if (p) memset(p, 0, t);
    return p;
}
void ph_free(void* p)
{
    if (!p) return;
    if (pool_free(p)) return;
    free(((ph_hdr*)p - 1)->raw);
}
void* ph_realloc(void* p, size_t size)
{
    void* q; size_t old;
    if (!p) return ph_malloc(size);
    if (size == 0) { ph_free(p); return NULL; }
    old = pool_size_of(p);
    if (!old) old = ((ph_hdr*)p - 1)->size;
    if (size <= old && old - size < 64) return p;
    q = ph_malloc(size);
    if (!q) return NULL;
    memcpy(q, p, old < size ? old : size);
    ph_free(p);
    return q;
}
