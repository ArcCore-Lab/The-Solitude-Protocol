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

#define ALIGN 8
#define PAGE_SIZE 4096
#define NUM_SIZE_CLASSES 30
#define MAX_SMALL_SIZE 256

#define MIN_BATCH_SIZE 10
#define MAX_BATCH_SIZE 100

#define SPAN_MAP_SIZE 1024

typedef struct SpanMapEntry {
    void *addr;
    Span *span;
    struct SpanMapEntry *next;
} SpanMapEntry;

typedef struct SpanMap {
    SpanMapEntry *buckets[SPAN_MAP_SIZE];
    pthread_mutex_t lock;
} SpanMap;

static SpanMap span_map;
static pthread_once_t span_map_init_once = PTHREAD_ONCE_INIT;

static void span_map_init(void) {
    memset(&span_map, 0, sizeof(SpanMap));
    pthread_mutex_init(&span_map.lock, NULL);
}

static inline size_t span_map_hash(void *addr) {
    return ((uintptr_t)addr >> 12) % SPAN_MAP_SIZE;
}

static void span_map_register(Span *span) {
    pthread_mutex_lock(&span_map.lock);
    
    size_t hash = span_map_hash(span->start);
    SpanMapEntry *entry = (SpanMapEntry *)malloc(sizeof(SpanMapEntry));
    entry->addr = span->start;
    entry->span = span;
    entry->next = span_map.buckets[hash];
    span_map.buckets[hash] = entry;
    
    pthread_mutex_unlock(&span_map.lock);
}

static Span *span_map_lookup(void *addr) {
    pthread_mutex_lock(&span_map.lock);
    
    size_t hash = span_map_hash(addr);
    SpanMapEntry *entry = span_map.buckets[hash];
    
    while (entry) {
        char *start = (char *)entry->span->start;
        char *end = start + entry->span->num_pages * PAGE_SIZE;
        
        if (addr >= (void *)start && addr < (void *)end) {
            pthread_mutex_unlock(&span_map.lock);
            return entry->span;
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&span_map.lock);
    return NULL;
}

static const size_t size_class_table[NUM_SIZE_CLASSES] = {
    // 小对象（高频）：颗粒度细分
    8, 16, 24, 32, 40, 48, 56, 64,
    72, 80, 88, 96, 104, 112, 120, 128,
    // 中对象：递增步长
    144, 160, 176, 192, 208, 224, 240, 256,
    // 大对象（低频）：指数增长
    512, 1024, 2048, 4096, 8192, 16384
};

static inline size_t size_to_class(size_t size) {
    if (size == 0) return 0;

    if (size <= 128) {
        return (size + 7) / 8 - 1;
    } else if (size <= 256) {
        return 16 + (size - 128 + 15) / 16;
    } else {
        size_t start = 24;
        size_t end = NUM_SIZE_CLASSES - 1;
        while (start < end) {
            size_t mid = (start + end) / 2;
            if (size_class_table[mid] <  size) {
                start = mid + 1;
            } else {
                end = mid;
            }
        }

        return start;
    }
}

typedef struct FreeList {
    void *head;
    size_t length;
    size_t max_length;
} FreeList;

static inline void fl_push(FreeList *fl, void *obj) {
    *(void **)obj = fl->head;
    fl->head = obj;
    fl->length++;
}

static inline void *fl_pop(FreeList *fl) {
    if (fl->head == NULL) return NULL;

    void *obj = fl->head;
    fl->head = *(void **)obj;
    fl->length--;

    return obj;
}

typedef struct Span {
    void *start;
    size_t num_pages;
    size_t size_class;

    void *freelist;
    size_t allocated;
    size_t total;

    struct Span *next;
    struct Span *prev;
} Span;

typedef struct TreadCache {
    FreeList bins[NUM_SIZE_CLASSES];
    size_t batch_size[NUM_SIZE_CLASSES];
    uint64_t alloc_count[NUM_SIZE_CLASSES];
    uint64_t miss_count[NUM_SIZE_CLASSES];
} ThreadCache;

static __thread ThreadCache *thread_cache = NULL;

static ThreadCache *tc_init(void) {
    ThreadCache *cache = (ThreadCache *)malloc(sizeof(ThreadCache));
    if (!cache) return NULL;

    memset(cache, 0, sizeof(ThreadCache));

    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        cache->batch_size[i] = MIN_BATCH_SIZE;
        cache->bins[i].max_length = MIN_BATCH_SIZE * 2;
    }

    return cache;
}

typedef struct CentralCache {
    Span *spans[NUM_SIZE_CLASSES];
    pthread_mutex_t locks[NUM_SIZE_CLASSES];
} CentralCache;

static CentralCache central_cache;
static pthread_once_t central_init_once = PTHREAD_ONCE_INIT;

static void central_cache_init(void) {
    memset(&central_cache, 0, sizeof(CentralCache));
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        pthread_mutex_init(&central_cache.locks[i], NULL);
    }
}

typedef struct PageHeap {
    pthread_mutex_t lock;
} PageHeap;

static PageHeap page_heap;
static pthread_once_t page_init_once = PTHREAD_ONCE_INIT;

static void page_heap_init(void) {
    pthread_mutex_init(&page_heap.lock, NULL);
}

static Span *page_heap_allocate(size_t num_pages) {
    pthread_mutex_lock(&page_heap.lock);

    size_t size = num_pages * PAGE_SIZE;
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED) {
        pthread_mutex_unlock(&page_heap.lock);
        return NULL;
    }

    Span *span = (Span *)malloc(sizeof(Span));
    span->start = mem;
    span->num_pages = num_pages;
    span->freelist = NULL;
    span->allocated = 0;
    span->total = 0;
    span->next = NULL;
    span->prev = NULL;

    pthread_mutex_unlock(&page_heap.lock);

    span_map_register(span);

    return span;
}

static void span_populate(Span *span, size_t size_class) {
    span->size_class = size_class;
    size_t obj_size = size_class_table[size_class];
    size_t total_size = span->num_pages * PAGE_SIZE;

    char *ptr = (char *)span->start;
    span->total = total_size / obj_size;

    for (size_t i = 0; i < span->total; i++) {
        *(void **)ptr = span->freelist;
        span->freelist = ptr;
        ptr += obj_size;
    }
}

static size_t central_cache_fetch(size_t size_class, void **out, size_t batch) {
    pthread_mutex_lock(&central_cache.locks[size_class]);
    
    Span *span = central_cache.spans[size_class];
    
    if (!span || !span->freelist) {
        span = page_heap_allocate(1);
        if (!span) {
            pthread_mutex_unlock(&central_cache.locks[size_class]);
            return 0;
        }
        
        span_populate(span, size_class);
        span->next = central_cache.spans[size_class];
        central_cache.spans[size_class] = span;
    }
    
    size_t count = 0;
    while (count < batch && span->freelist) {
        void *obj = span->freelist;
        span->freelist = *(void **)obj;
        out[count++] = obj;
        span->allocated++;
    }
    
    pthread_mutex_unlock(&central_cache.locks[size_class]);
    return count;
}
 
static void central_cache_release(size_t size_class, void **objs, size_t count) {
    pthread_mutex_lock(&central_cache.locks[size_class]);
    
    Span *span = central_cache.spans[size_class];
    if (!span) {
        pthread_mutex_unlock(&central_cache.locks[size_class]);
        return;
    }
    
    for (size_t i = 0; i < count; i++) {
        *(void **)objs[i] = span->freelist;
        span->freelist = objs[i];
        span->allocated--;
    }
    
    pthread_mutex_unlock(&central_cache.locks[size_class]);
}

static void *thread_cache_refill(ThreadCache *cache, size_t size_class) {
    size_t batch = cache->batch_size[size_class];
    void *objs[MAX_BATCH_SIZE];
    
    size_t fetched = central_cache_fetch(size_class, objs, batch);
    if (fetched == 0) return NULL;
    
    void *result = objs[0];
    
    for (size_t i = 1; i < fetched; i++) {
        fl_push(&cache->bins[size_class], objs[i]);
    }
    
    cache->miss_count[size_class]++;
    return result;
}

static void thread_cache_release_excess(ThreadCache *cache, size_t size_class) {
    FreeList *fl = &cache->bins[size_class];
    
    if (fl->length <= fl->max_length) return;
    
    size_t release_count = fl->length / 2;
    void *objs[MAX_BATCH_SIZE];
    size_t count = 0;
    
    while (count < release_count && count < MAX_BATCH_SIZE) {
        objs[count++] = fl_pop(fl);
    }
    
    central_cache_release(size_class, objs, count);
}

void *tc_malloc(size_t size) {
    if (size == 0) return NULL;
    if (size > MAX_SMALL_SIZE) {
        size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        return mmap(NULL, pages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    pthread_once(&central_init_once, central_cache_init);
    pthread_once(&page_init_once, page_heap_init);
    pthread_once(&span_map_init_once, span_map_init);

    if (!thread_cache) {
        thread_cache = tc_init();
        if (!thread_cache) return NULL;
    }
    
    size_t clas = size_to_class(size);
    thread_cache->alloc_count[clas]++;
    
    void *obj = fl_pop(&thread_cache->bins[clas]);
    if (obj) return obj;
    
    return thread_cache_refill(thread_cache, clas);
}

void tc_free(void *ptr) {
    if (!ptr) return;
    
    if (!thread_cache) {
        thread_cache = tc_init();
        if (!thread_cache) {
            free(ptr);
            return;
        }
    }
    
    Span *span = span_map_lookup(ptr);
    
    if (!span) {
        munmap(ptr, PAGE_SIZE);
        return;
    }
    
    size_t size_class = span->size_class;
    
    fl_push(&thread_cache->bins[size_class], ptr);
    
    thread_cache_release_excess(thread_cache, size_class);
}

void tc_print_stats(void) {
    if (!thread_cache) return;
    
    printf("=== ThreadCache Statistics ===\n");
    uint64_t total_alloc = 0;
    uint64_t total_miss = 0;
    
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (thread_cache->alloc_count[i] > 0) {
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

void tc_cleanup(void) {
    if (!thread_cache) return;
    
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        FreeList *fl = &thread_cache->bins[i];
        if (fl->length > 0) {
            void *objs[MAX_BATCH_SIZE];
            size_t count = 0;
            
            while (fl->head && count < MAX_BATCH_SIZE) {
                objs[count++] = fl_pop(fl);
            }
            
            if (count > 0) {
                central_cache_release(i, objs, count);
            }
        }
    }
    
    free(thread_cache);
    thread_cache = NULL;
}

#endif /*THREAD_CACHE_H*/