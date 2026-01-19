#ifndef TCMALLOC_H
#define TCMALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include "unp_day07.h"

#define ALIGN 8
#define MAX_BYTES 128
#define ARENA_SIZE 1024
#define NFREELISTS (MAX_BYTES/ALIGN)

typedef struct {
    void *head;
} FreeList;

typedef struct {
    char *start;
    char *next;
    char *end;

    FreeList free_lists[NFREELISTS];
    pthread_mutex_t lock;
} MemoryPool;

static size_t get_bucket_index(size_t size) {
    return (size + ALIGN - 1) / ALIGN - 1;
}

static size_t round_up(size_t size) {
    if (size > (size_t)-ALIGN) return 0;
    return (size + ALIGN - 1) & ~(ALIGN - 1);
}

void *fill_bucket(MemoryPool *pool, size_t size) {
    int n_blocks = 20;
    size_t total_needed = size * n_blocks;

    if (pool->next + total_needed > pool->end) {
        n_blocks = (pool->end - pool->next) / size;
        if (n_blocks == 0) return NULL;
    }

    void *result = pool->next;
    pool->next += size;

    if (n_blocks > 1) {
        size_t index = get_bucket_index(size);
        void *current = pool->next;
        pool->free_lists[index].head = current;

        for (int i = 0; i < n_blocks - 2; i++) {
            void *next_block = (char *)current + size;
            *(void **)current = next_block;
            current = next_block;
        }

        *(void **)current = NULL;
        pool->next += size * (n_blocks - 1);
    }

    return result;
}

void fl_push(FreeList *fl, void *block) {
    *(void **)block = fl->head;
    fl->head = block;
}

void *fl_pop(FreeList *fl) {
    if (fl->head == NULL) return NULL;

    void *block = fl->head;
    fl->head = *(void **)block;

    return block;
}

void pool_init(MemoryPool *pool) {
    pool->start = (char *)malloc(ARENA_SIZE);
    pool->next = pool->start;
    pool->end = pool->start + ARENA_SIZE;

    for (int i = 0; i < NFREELISTS; i++) {
        pool->free_lists[i].head = NULL;
    }

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        perror("Mutex init fails");
        exit(1);
    }
}

void *pool_alloc(MemoryPool *pool, size_t size)
{
    size_t alloc_size = round_up(size + sizeof(size_t));
    size_t index = get_bucket_index(alloc_size);

    pthread_mutex_lock(&pool->lock);

    void *block = fl_pop(&pool->free_lists[index]);

    if (!block) {
        block = fill_bucket(pool, alloc_size);
    }

    if (block) {
        *(size_t *)block = alloc_size;
    }

    pthread_mutex_unlock(&pool->lock);

    return block ? (char *)block + sizeof(size_t) : NULL;
}

void pool_free(MemoryPool *pool, void *p)
{
    if (!p)
        return;

    void *real_block = (char *)p - sizeof(size_t);

    if (real_block < (void *)pool->start || real_block >= (void *)pool->end)
        return;

    size_t alloc_size = *(size_t *)real_block;
    size_t index = get_bucket_index(alloc_size);

    pthread_mutex_lock(&pool->lock);
    fl_push(&pool->free_lists[index], real_block);
    pthread_mutex_unlock(&pool->lock);
}

void pool_destroy(MemoryPool *pool)
{
    if (pool->start)
    {
        free(pool->start);
    }
    pthread_mutex_destroy(&pool->lock);
}

#endif /*TCMALLOC_H*/