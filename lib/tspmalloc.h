/*
    * tspmalloc.h
    *
    * Thread-safe memory allocator for small fixed-size blocks.
    *
    * This allocator uses a memory pool divided into arenas and free lists
    * for efficient allocation and deallocation of memory blocks up to 128 bytes.
    *
    * According to the TCMalloc design.
*/

#ifndef TSPMALLOC_H
#define TSPMALLOC_H

#include "include/unp.h"
#include <pthread.h>

/*
    * As we all know, tcmalloc has three important mindsets:
    * 1. Hierarchical Allocation:
    *  - Thread Cache: Each thread has its own cache for fast allocation/deallocation.
    *  - Central Cache: Shared among threads, manages larger memory blocks.
    *  - Page Heap: Manages large memory allocations from the OS.
    * 2. Size Classes:
    *  - Memory is divided into size classes to reduce fragmentation and improve allocation speed.
    * 3. Embedded Pointer:
    *  - Free list nodes are embedded within the memory blocks themselves to minimize overhead.
    *  - Pointer when free, data when allocated.
    *
    * In this simplified version, I achieved mixed and simplified versions of "Thread Cache" and "Central Cache".
    * The key components are:
    * 1. Hierarchical Allocation: MemoryPool -- simulates a combined Thread Cache and Central Cache.
    * 2. Size Classes: FreeList -- manages free blocks of specific sizes.
    * 3. Embedded Pointer: FreeList nodes are embedded within the allocated blocks.
*/

#define ALIGN 8
#define MAX_BYTES 128
#define ARENA_SIZE 1024
#define NFREELISTS (MAX_BYTES/ALIGN)

/* Free list structure */
typedef struct {
    void *head;
} FreeList;

/* Memory pool structure */
typedef struct {
    void *start;
    void *next;
    void *end;

    FreeList fls[NFREELISTS];
    pthread_mutex_t lock;
} MemoryPool;

/*
    Get the index of the free list bucket for a given size.
*/
static size_t get_bucket_index(size_t size) {
    return (size + ALIGN - 1) / ALIGN - 1;
}

/* 
    Round up size to the nearest multiple of ALIGN.
*/
static size_t round_up(size_t size) {
    if (size > ((size_t) - ALIGN)) return 0;
    return (size + ALIGN - 1) & ~(ALIGN - 1);
}

/*
    Push a block onto the free list.
*/
void fl_push(FreeList *fl, void *block) {
    *(void **)block = fl->head;
    fl->head = block;
}

/*
    Pop a block from the free list.
*/
void *fl_pop(FreeList *fl) {
    if (fl->head == NULL) return NULL;

    void *block = fl->head;
    fl->head = *(void **)block;

    return block;
}

/*
    Fill the bucket with new blocks from the memory pool.
*/
void *fill_bucket(MemoryPool *pool, size_t size) {
    int n_blocks = 20;
    size_t total = size * n_blocks;

    if (pool->next + total > pool->end) {
        n_blocks = (pool->end - pool->next) / size;
        
        if (n_blocks == 0) return NULL; 
    }

    void *result = pool->next;
    pool->next += size;

    if (n_blocks > 1) {
        size_t index = get_bucket_index(size);
        void *current = pool->next;
        pool->fls[index].head = current;

        for (int i = 0; i < n_blocks - 2; i++) {
            void *next = (char *)current + size;
            *(void **)current = next;
            current = next;
        }

        *(void **)current = NULL;
        pool->next += size * (n_blocks - 1);
    }

    return result;
}

/*
    Initialize the memory pool.
*/
void tsp_init(MemoryPool *pool) {
    pool->start = (char *)malloc(ARENA_SIZE);
    pool->next = pool->start;
    pool->end = pool->start + ARENA_SIZE;

    for (int i = 0; i < NFREELISTS; i++) {
        pool->fls[i].head = NULL;
    }

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        perror("Mutex init failed");
        exit(1);
    }
}

/*
    Destroy the memory pool and free resources.
*/
void tsp_destroy(MemoryPool *pool) {
    if (!pool) return;

    pthread_mutex_lock(&pool->lock);

    if (pool->start){
        free(pool->start);
        pool->start = NULL;
    }

    pthread_mutex_unlock(&pool->lock);

    pthread_mutex_destroy(&pool->lock);
}

/*
    Allocate memory from the pool.
*/
void *tspmalloc(MemoryPool *pool, size_t size) {
    size_t alloc_size = round_up(size + sizeof(size_t));
    size_t index = get_bucket_index(alloc_size);

    pthread_mutex_lock(&pool->lock);

    void *block = fl_pop(&pool->fls[index]);

    if (!block) block = fill_bucket(pool, alloc_size);

    if (block) *(size_t *)block = alloc_size;

    pthread_mutex_unlock(&pool->lock);

    return block ? (char *)block + sizeof(size_t) : NULL;
}

/*
    Free memory back to the pool.
*/
void tspfree(MemoryPool *pool, void *p) {
    if (!p) return;

    void *real_block = (char *)p - sizeof(size_t);
    if (real_block < (void *)pool->start || real_block >= (void *)pool->end)
        return;

    size_t alloc_size = *(size_t *)real_block;
    size_t index = get_bucket_index(alloc_size);

    pthread_mutex_lock(&pool->lock);
    fl_push(&pool->fls[index], real_block);
    pthread_mutex_unlock(&pool->lock);
}

#endif /*TSPMALLOC_H*/