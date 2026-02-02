#ifndef THREAD_CACHE_H
#define THREAD_CACHE_H

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

static const size_t size_class_table[NUM_SIZE_CLASSES] = {
    8, 16, 24, 32, 40, 48, 56, 64,
    72, 80, 88, 96, 104, 112, 120, 128,
    144, 160, 176, 192, 208, 224, 240, 256,
    512, 1024, 2048, 4096, 8192, 16384
};

static inline size_t size_to_class(size_t size) {
    if (size == 0) return 0;

    if (size <= 128) {
        return (size + 7) / 8 - 1;
    } else if (size <= 256) {
        return 16 + (size - 128) / 16;
    } else {
        size_t start = 24;
        size_t end = NUM_SIZE_CLASSES - 1;
        while (start <= end) {
            size_t mid = (start + end) / 2;
            if (size_class_table[mid] <  size) {
                start = mid + 1;
            } else if (size_class_table[mid] > size) {
                end = mid - 1;
            } else {
                return mid;
            }
        }
    }

    return NUM_SIZE_CLASSES - 1;
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

static void cantral_cache_init(void) {
    memset(&central_cache, 0, sizeof(CentralCache));
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        pthread_mutex_init(&central_cache.locks[i], NULL);
    }
}

#endif /*THREAD_CACHE_H*/