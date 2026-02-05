#define _POSIX_C_SOURCE 199309L

#include "thread_cache.h"
#include <pthread.h>
#include <time.h>
#include <assert.h>

#define NUM_THREADS 8
#define OPS_PER_THREAD 100000

#ifdef USE_GLIBC
    #define tc_malloc malloc
    #define tc_free free
    #define tc_cleanup() ((void)0)
    #define tc_print_stats() ((void)0)
#endif

typedef struct {
    int thread_id;
    uint64_t duration_ns;
} ThreadResult;

void *worker_thread(void *arg) {
    ThreadResult *result = (ThreadResult *)arg;
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);

    void *ptrs[100];
    memset(ptrs, 0, sizeof(ptrs));

    for (int i = 0; i < OPS_PER_THREAD; i++) {
        int r = rand() % 100;
        size_t size;
        
        if (r < 80) {
            size = 8 + (rand() % 120);
        } else if (r < 95) {
            size = 128 + (rand() % 128);
        } else {
            size = 512 + (rand() % 512);
        }

        int idx = i % 100;
        if (ptrs[idx]) {
            tc_free(ptrs[idx]);
            ptrs[idx] = NULL;
        }
        
        ptrs[idx] = tc_malloc(size);
        if (ptrs[idx] == NULL) {
            printf("分配失败: size=%zu\n", size);
            continue;
        }
        
        memset(ptrs[idx], 0xAB, size);
        
        if (rand() % 10 < 3 && i > 0) {
            int prev_idx = (i - 1) % 100;
            if (ptrs[prev_idx]) {
                tc_free(ptrs[prev_idx]);
                ptrs[prev_idx] = NULL;
            }
        }
    }
    
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) {
            tc_free(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    result->duration_ns = (end.tv_sec - start.tv_sec) * 1000000000UL +
                          (end.tv_nsec - start.tv_nsec);
    
    tc_print_stats();
    // tc_cleanup();
    
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    ThreadResult results[NUM_THREADS];
    struct timespec start, end;
    
    printf("🔧 启动 ThreadCache 压测...\n");
    printf("线程数: %d\n", NUM_THREADS);
    printf("每线程操作数: %d\n", OPS_PER_THREAD);
    printf("总操作数: %d\n\n", NUM_THREADS * OPS_PER_THREAD);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        results[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread, &results[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    uint64_t total_duration = (end.tv_sec - start.tv_sec) * 1000000000UL +
                              (end.tv_nsec - start.tv_nsec);
    
    printf("\n=== 性能报告 ===\n");
    printf("总耗时: %.3f 秒\n", total_duration / 1e9);
    printf("总吞吐: %.2f M ops/s\n", 
           (NUM_THREADS * OPS_PER_THREAD) / (total_duration / 1e9) / 1e6);
    
    uint64_t avg_thread_ns = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        avg_thread_ns += results[i].duration_ns;
        printf("线程 %d: %.2f M ops/s\n", 
               i, OPS_PER_THREAD / (results[i].duration_ns / 1e9) / 1e6);
    }
    
    avg_thread_ns /= NUM_THREADS;
    printf("\n平均单线程吞吐: %.2f M ops/s\n", 
           OPS_PER_THREAD / (avg_thread_ns / 1e9) / 1e6);
    printf("平均延迟: %.0f ns/op\n", (double)avg_thread_ns / OPS_PER_THREAD);
    
    return 0;
}