#include <stdint.h>
#include "paging.h"
#include "pmm.h"

#define HEAP_START 0xD0000000
#define HEAP_SIZE  (16 * 1024 * 1024)

typedef struct heap_block {
    uint32_t size;
    uint32_t free;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head = NULL;

void kmalloc_init(void) {
    // 映射堆空间
    for (uint32_t i = 0; i < HEAP_SIZE; i += PAGE_SIZE) {
        uint32_t paddr = pmm_alloc_page();
        map_page(HEAP_START + i, paddr, PTE_PRESENT | PTE_RW);
    }
    
    heap_head = (heap_block_t *)HEAP_START;
    heap_head->size = HEAP_SIZE - sizeof(heap_block_t);
    heap_head->free = 1;
    heap_head->next = NULL;
}

void *kmalloc(uint32_t size) {
    // 字节对齐
    size = (size + 7) & ~7;
    
    heap_block_t *current = heap_head;
    while (current) {
        if (current->free && current->size >= size) {
            // 分割块
            if (current->size > size + sizeof(heap_block_t) + 8) {
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)current + sizeof(heap_block_t) + size);
                new_block->size = current->size - size - sizeof(heap_block_t);
                new_block->free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->free = 0;
            return (void *)((uint8_t *)current + sizeof(heap_block_t));
        }
        current = current->next;
    }
    
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;
    
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
    block->free = 1;
    
    heap_block_t *current = heap_head;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += sizeof(heap_block_t) + current->next->size;
            current->next = current->next->next;
        }
        current = current->next;
    }
}