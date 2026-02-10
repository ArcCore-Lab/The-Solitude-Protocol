#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 4096
#define TOTAL_MEMORY (32 * 1024 * 1024)
#define BITMAP_SIZE (TOTAL_MEMORY / PAGE_SIZE / 8)

static uint8_t page_bitmap[BITMAP_SIZE];
static uint32_t total_pages;
static uint32_t used_pages;

void pmm_init(uint32_t mem_size) {
    total_pages = mem_size / PAGE_SIZE;
    used_pages = 0;
    memset(page_bitmap, 0, BITMAP_SIZE);
    
    for (uint32_t i = 0; i < 256 + 64; i++) { 
        page_bitmap[i / 8] |= (1 << (i % 8));
        used_pages++;
    }
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t byte = i / 8;
        uint32_t bit = i % 8;
        
        if (!(page_bitmap[byte] & (1 << bit))) {
            page_bitmap[byte] |= (1 << bit);
            used_pages++;
            return i * PAGE_SIZE;
        }
    }
    return 0;  
}

void pmm_free_page(uint32_t paddr) {
    uint32_t page = paddr / PAGE_SIZE;
    page_bitmap[page / 8] &= ~(1 << (page % 8));
    used_pages--;
}