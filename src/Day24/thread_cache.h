#ifndef THREAD_CACHE_H
#define THREAD_CACHE_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdatomic.h>

#define ALIGN 8
#define PAGE_SIZE 4096
#define NUM_SIZE_CLASSES 30
#define MAX_SMALL_SIZE 256

#define MIN_BATCH_SIZE 32
#define MAX_BATCH_SIZE 512

#define MAX_SPAN_PAGES 262144

static uint8_t fast_size_map[257];
static int size_map_initialized = 0;

static const size_t optimal_batch_sizes[NUM_SIZE_CLASSES] = {
    256, 256, 256, 256, 256, 256, 256, 256,
    192, 192, 192, 192, 192, 192, 192, 128,
    96, 96, 64, 64, 64, 64, 64, 64,
    32, 16, 8, 4, 2, 1};

static const size_t size_class_table[NUM_SIZE_CLASSES] = {
    8, 16, 24, 32, 40, 48, 56, 64,
    72, 80, 88, 96, 104, 112, 120, 128,
    144, 160, 176, 192, 208, 224, 240, 256,
    512, 1024, 2048, 4096, 8192, 16384};

static void init_fast_size_map(void)
{
    if (size_map_initialized)
        return;

    for (size_t i = 0; i <= 128; i++)
    {
        fast_size_map[i] = ((i + 7) / 8 > 0) ? ((i + 7) / 8 - 1) : 0;
    }

    for (size_t i = 129; i <= 256; i++)
    {
        size_t class_id = 16 + (i - 128 + 15) / 16;
        fast_size_map[i] = (class_id < NUM_SIZE_CLASSES) ? class_id : NUM_SIZE_CLASSES - 1;
    }

    size_map_initialized = 1;
}

static inline size_t size_to_class(size_t size)
{
    if (size == 0)
        return 0;

    if (__builtin_expect(!size_map_initialized, 0))
    {
        init_fast_size_map();
    }

    if (__builtin_expect(size <= 256, 1))
    {
        return fast_size_map[size];
    }

    if (size <= 512)
        return 24;
    if (size <= 1024)
        return 25;
    if (size <= 2048)
        return 26;
    if (size <= 4096)
        return 27;
    if (size <= 8192)
        return 28;
    return 29;
}

typedef struct FreeList
{
    void *head;
    size_t length;
    size_t max_length;
} FreeList;

static inline void fl_push(FreeList *fl, void *obj)
{
    *(void **)obj = fl->head;
    fl->head = obj;
    fl->length++;
}

static inline void *fl_pop(FreeList *fl)
{
    if (fl->head == NULL)
        return NULL;

    void *obj = fl->head;
    fl->head = *(void **)obj;
    fl->length--;

    return obj;
}

typedef struct Span
{
    void *start;
    size_t num_pages;
    size_t size_class;

    atomic_uintptr_t freelist;
    atomic_size_t allocated;
    size_t total;

    struct Span *next;
} Span;

typedef struct ThreadCache
{
    FreeList bins[NUM_SIZE_CLASSES];
    size_t batch_size[NUM_SIZE_CLASSES];
    uint64_t alloc_count[NUM_SIZE_CLASSES];
    uint64_t miss_count[NUM_SIZE_CLASSES];
} ThreadCache;

static __thread ThreadCache *thread_cache = NULL;

static ThreadCache *tc_init(void)
{
    ThreadCache *cache = (ThreadCache *)malloc(sizeof(ThreadCache));
    if (!cache)
        return NULL;

    memset(cache, 0, sizeof(ThreadCache));

    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++)
    {
        cache->batch_size[i] = optimal_batch_sizes[i];
        cache->bins[i].max_length = optimal_batch_sizes[i] * 4;
    }

    return cache;
}

typedef struct CentralCache
{
    Span *spans[NUM_SIZE_CLASSES];
    atomic_uintptr_t hot_span[NUM_SIZE_CLASSES];
    pthread_mutex_t locks[NUM_SIZE_CLASSES];
} CentralCache;

static CentralCache central_cache;
static pthread_once_t central_init_once = PTHREAD_ONCE_INIT;

static void central_cache_init(void)
{
    memset(&central_cache, 0, sizeof(CentralCache));
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++)
    {
        pthread_mutex_init(&central_cache.locks[i], NULL);
        atomic_init(&central_cache.hot_span[i], 0);
    }
}

typedef struct FastSpanMap
{
    atomic_uintptr_t page_to_span[MAX_SPAN_PAGES];
} FastSpanMap;

static FastSpanMap fast_span_map;
static pthread_once_t span_map_init_once = PTHREAD_ONCE_INIT;

static void fast_span_map_init(void)
{
    for (size_t i = 0; i < MAX_SPAN_PAGES; i++)
    {
        atomic_init(&fast_span_map.page_to_span[i], 0);
    }
}

static inline void fast_span_register(Span *span)
{
    uintptr_t page_start = (uintptr_t)span->start / PAGE_SIZE;
    size_t num_pages = span->num_pages;

    for (size_t i = 0; i < num_pages; i++)
    {
        size_t page_id = page_start + i;
        if (page_id < MAX_SPAN_PAGES)
        {
            atomic_store_explicit(&fast_span_map.page_to_span[page_id],
                                  (uintptr_t)span, memory_order_release);
        }
    }
}

static inline Span *fast_span_lookup(void *addr)
{
    uintptr_t page_id = (uintptr_t)addr / PAGE_SIZE;

    if (__builtin_expect(page_id >= MAX_SPAN_PAGES, 0))
        return NULL;

    uintptr_t span_ptr = atomic_load_explicit(&fast_span_map.page_to_span[page_id],
                                              memory_order_relaxed);
    return (Span *)span_ptr;
}

typedef struct PageHeap
{
    pthread_mutex_t lock;
} PageHeap;

static PageHeap page_heap;
static pthread_once_t page_init_once = PTHREAD_ONCE_INIT;

static void page_heap_init(void)
{
    pthread_mutex_init(&page_heap.lock, NULL);
}

static Span *page_heap_allocate(size_t num_pages)
{
    pthread_mutex_lock(&page_heap.lock);

    size_t size = num_pages * PAGE_SIZE;
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED)
    {
        pthread_mutex_unlock(&page_heap.lock);
        return NULL;
    }

    Span *span = (Span *)malloc(sizeof(Span));
    span->start = mem;
    span->num_pages = num_pages;
    atomic_init(&span->freelist, 0);
    atomic_init(&span->allocated, 0);
    span->total = 0;
    span->next = NULL;

    pthread_mutex_unlock(&page_heap.lock);

    fast_span_register(span);

    return span;
}

static void span_populate(Span *span, size_t size_class)
{
    span->size_class = size_class;
    size_t obj_size = size_class_table[size_class];
    size_t total_size = span->num_pages * PAGE_SIZE;

    char *ptr = (char *)span->start;
    span->total = total_size / obj_size;

    void *freelist = NULL;
    for (size_t i = 0; i < span->total; i++)
    {
        *(void **)ptr = freelist;
        freelist = ptr;
        ptr += obj_size;
    }

    atomic_store_explicit(&span->freelist, (uintptr_t)freelist, memory_order_release);
}

static size_t span_fetch_batch(Span *span, void **out, size_t batch)
{
    size_t count = 0;
    void *curr = (void *)atomic_load_explicit(&span->freelist, memory_order_acquire);

    while (count < batch && curr)
    {
        void *next = *(void **)curr;
        out[count++] = curr;
        curr = next;
    }

    if (count > 0)
    {
        atomic_store_explicit(&span->freelist, (uintptr_t)curr, memory_order_release);
        atomic_fetch_add_explicit(&span->allocated, count, memory_order_relaxed);
    }

    return count;
}

static size_t central_cache_fetch(size_t size_class, void **out, size_t batch)
{
    Span *hot = (Span *)atomic_load_explicit(&central_cache.hot_span[size_class],
                                             memory_order_acquire);

    if (hot)
    {
        void *freelist = (void *)atomic_load_explicit(&hot->freelist, memory_order_acquire);
        if (freelist)
        {
            size_t count = span_fetch_batch(hot, out, batch);
            if (count > 0)
                return count;
        }
    }

    pthread_mutex_lock(&central_cache.locks[size_class]);

    Span *span = central_cache.spans[size_class];

    if (!span || !atomic_load_explicit(&span->freelist, memory_order_acquire))
    {
        pthread_mutex_unlock(&central_cache.locks[size_class]);

        Span *new_span = page_heap_allocate(1);
        if (!new_span)
            return 0;

        span_populate(new_span, size_class);

        pthread_mutex_lock(&central_cache.locks[size_class]);

        new_span->next = central_cache.spans[size_class];
        central_cache.spans[size_class] = new_span;
        atomic_store_explicit(&central_cache.hot_span[size_class],
                              (uintptr_t)new_span, memory_order_release);
        span = new_span;
    }

    size_t count = span_fetch_batch(span, out, batch);

    pthread_mutex_unlock(&central_cache.locks[size_class]);
    return count;
}

static void central_cache_release(size_t size_class, void **objs, size_t count)
{
    if (size_class >= NUM_SIZE_CLASSES || count == 0)
        return;

    Span *hot = (Span *)atomic_load_explicit(&central_cache.hot_span[size_class],
                                             memory_order_acquire);

    if (hot)
    {
        for (size_t i = 0; i < count; i++)
        {
            void *obj = objs[i];
            if (!obj)
                continue;

            uintptr_t old_head = atomic_load_explicit(&hot->freelist, memory_order_acquire);
            uintptr_t new_head;
            do
            {
                *(void **)obj = (void *)old_head;
                new_head = (uintptr_t)obj;
            } while (!atomic_compare_exchange_weak_explicit(&hot->freelist, &old_head, new_head,
                                                            memory_order_release,
                                                            memory_order_acquire));

            atomic_fetch_sub_explicit(&hot->allocated, 1, memory_order_relaxed);
        }
        return;
    }

    pthread_mutex_lock(&central_cache.locks[size_class]);

    Span *span = central_cache.spans[size_class];
    if (!span)
    {
        pthread_mutex_unlock(&central_cache.locks[size_class]);
        return;
    }

    void *freelist = (void *)atomic_load_explicit(&span->freelist, memory_order_acquire);
    for (size_t i = 0; i < count; i++)
    {
        void *obj = objs[i];
        if (!obj)
            continue;

        *(void **)obj = freelist;
        freelist = obj;
    }
    atomic_store_explicit(&span->freelist, (uintptr_t)freelist, memory_order_release);
    atomic_fetch_sub_explicit(&span->allocated, count, memory_order_relaxed);

    pthread_mutex_unlock(&central_cache.locks[size_class]);
}

static void *thread_cache_refill(ThreadCache *cache, size_t size_class)
{
    size_t batch = cache->batch_size[size_class];
    void *objs[MAX_BATCH_SIZE];

    size_t fetched = central_cache_fetch(size_class, objs, batch);
    if (fetched == 0)
        return NULL;

    void *result = objs[0];

    for (size_t i = 1; i < fetched; i++)
    {
        fl_push(&cache->bins[size_class], objs[i]);
    }

    cache->miss_count[size_class]++;
    return result;
}

static void thread_cache_release_excess(ThreadCache *cache, size_t size_class)
{
    FreeList *fl = &cache->bins[size_class];

    if (fl->length <= fl->max_length)
        return;

    size_t release_count = fl->length / 2;
    void *objs[MAX_BATCH_SIZE];
    size_t count = 0;

    while (count < release_count && count < MAX_BATCH_SIZE && fl->head)
    {
        objs[count++] = fl_pop(fl);
    }

    if (count > 0)
    {
        central_cache_release(size_class, objs, count);
    }
}

static volatile int g_init_flag = 0;
static pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;

static inline void ensure_init(void)
{
    if (__builtin_expect(__atomic_load_n(&g_init_flag, __ATOMIC_ACQUIRE) == 0, 0))
    {
        pthread_mutex_lock(&g_init_lock);
        if (g_init_flag == 0)
        {
            central_cache_init();
            page_heap_init();
            fast_span_map_init();

            for (size_t i = 0; i < 16; i++)
            {
                Span *span = page_heap_allocate(1);
                if (span)
                {
                    span_populate(span, i);
                    central_cache.spans[i] = span;
                    atomic_store_explicit(&central_cache.hot_span[i],
                                          (uintptr_t)span, memory_order_release);
                }
            }

            __atomic_store_n(&g_init_flag, 1, __ATOMIC_RELEASE);
        }
        pthread_mutex_unlock(&g_init_lock);
    }
}

void *tc_malloc(size_t size)
{
    if (__builtin_expect(size == 0, 0))
        return NULL;

    if (__builtin_expect(size > MAX_SMALL_SIZE, 0))
    {
        size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        return mmap(NULL, pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    ensure_init();

    ThreadCache *cache = thread_cache;
    if (__builtin_expect(!cache, 0))
    {
        if (!size_map_initialized)
            init_fast_size_map();
        cache = thread_cache = tc_init();
        if (!cache)
            return NULL;
    }

    size_t clas = size_to_class(size);
    cache->alloc_count[clas]++;

    void *obj = fl_pop(&cache->bins[clas]);
    if (__builtin_expect(obj != NULL, 1))
    {
        return obj;
    }

    return thread_cache_refill(cache, clas);
}

void tc_free(void *ptr)
{
    if (__builtin_expect(!ptr, 0))
        return;

    uintptr_t addr = (uintptr_t)ptr;

    if (__builtin_expect(addr < 4096 || (addr & 0x7) != 0, 0))
    {
        return;
    }

    ensure_init();

    ThreadCache *cache = thread_cache;
    if (__builtin_expect(!cache, 0))
    {
        cache = thread_cache = tc_init();
        if (!cache)
            return;
    }

    Span *span = fast_span_lookup(ptr);

    if (__builtin_expect(!span, 0))
    {
        return;
    }

    size_t size_class = span->size_class;

    if (__builtin_expect(size_class >= NUM_SIZE_CLASSES, 0))
    {
        return;
    }

    fl_push(&cache->bins[size_class], ptr);

    if (__builtin_expect(cache->bins[size_class].length > cache->bins[size_class].max_length, 0))
    {
        thread_cache_release_excess(cache, size_class);
    }
}

void tc_print_stats(void)
{
    if (!thread_cache)
        return;

    printf("=== ThreadCache Statistics ===\n");
    uint64_t total_alloc = 0;
    uint64_t total_miss = 0;

    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++)
    {
        if (thread_cache->alloc_count[i] > 0)
        {
            total_alloc += thread_cache->alloc_count[i];
            total_miss += thread_cache->miss_count[i];

            double hit_rate = 100.0 * (1.0 - (double)thread_cache->miss_count[i] / thread_cache->alloc_count[i]);

            printf("Class %2zu (%5zu bytes): alloc=%8lu, miss=%6lu (%.1f%% hit), cached=%zu\n",
                   i, size_class_table[i],
                   thread_cache->alloc_count[i],
                   thread_cache->miss_count[i],
                   hit_rate,
                   thread_cache->bins[i].length);
        }
    }

    double overall_hit = 100.0 * (1.0 - (double)total_miss / total_alloc);
    printf("\n总计: 分配=%lu 次, 未命中=%lu 次, 命中率=%.2f%%\n",
           total_alloc, total_miss, overall_hit);
}

void tc_cleanup(void)
{
    if (!thread_cache)
        return;

    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++)
    {
        FreeList *fl = &thread_cache->bins[i];
        if (fl->length > 0)
        {
            void *objs[MAX_BATCH_SIZE];
            size_t count = 0;

            while (fl->head && count < MAX_BATCH_SIZE)
            {
                objs[count++] = fl_pop(fl);
            }

            if (count > 0)
            {
                central_cache_release(i, objs, count);
            }
        }
    }

    free(thread_cache);
    thread_cache = NULL;
}

#endif /*THREAD_CACHE_H*/