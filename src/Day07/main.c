#include "tcmalloc.h"
#include <assert.h>

int main()
{
    MemoryPool pool;
    pool_init(&pool);

    printf("--- Test 1: Simple Alloc/Free ---\n");
    void *p1 = pool_alloc(&pool, 10); // 10+8=18 -> round_up -> 24B
    assert(p1 != NULL);

    // write something to p1 to verify it's usable
    snprintf((char *)p1, 10, "hello");

    pool_free(&pool, p1);
    printf("Test 1 passed.\n");

    printf("--- Test 2: Reuse logic ---\n");
    void *p2 = pool_alloc(&pool, 10);
    // if the reuse logic works, p2 should be the same as p1
    if (p1 == p2)
    {
        printf("Success: Memory reused!\n");
    }

    printf("--- Test 3: Fill bucket trigger ---\n");
    void *ptrs[30];
    for (int i = 0; i < 30; i++)
    {
        ptrs[i] = pool_alloc(&pool, 8); // 8+8=16B
    }
    for (int i = 0; i < 30; i++)
    {
        pool_free(&pool, ptrs[i]);
    }
    printf("Test 3 passed.\n");

    pool_destroy(&pool);
    return 0;
}