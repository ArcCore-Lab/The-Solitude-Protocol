#include "paging.h"
#include "pmm.h"
#include <string.h>

static pde_t page_directory[1024] __attribute__((aligned(4096)));

extern void load_page_directory(uint32_t);
extern void enable_paging(void);

void paging_init(void) {
    memset(page_directory, 0, sizeof(page_directory));
    
    for (uint32_t i = 0; i < 1024; i++) {
        map_page(i * PAGE_SIZE, i * PAGE_SIZE, PTE_PRESENT | PTE_RW);
    }
    
    load_page_directory((uint32_t)page_directory);
    enable_paging();
}

void map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x3FF;
    
    pte_t *page_table;
    if (!(page_directory[pd_idx] & PDE_PRESENT)) {
        uint32_t pt_phys = pmm_alloc_page();
        page_directory[pd_idx] = pt_phys | PDE_PRESENT | PDE_RW | PDE_USER;
        
        page_table = (pte_t *)(pt_phys);
        memset(page_table, 0, PAGE_SIZE);
    } else {
        page_table = (pte_t *)(page_directory[pd_idx] & 0xFFFFF000);
    }
    
    page_table[pt_idx] = (paddr & 0xFFFFF000) | flags;
}

uint32_t get_physical_address(uint32_t vaddr) {
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x3FF;
    uint32_t offset = vaddr & 0xFFF;
    
    if (!(page_directory[pd_idx] & PDE_PRESENT)) {
        return 0;
    }
    
    pte_t *page_table = (pte_t *)(page_directory[pd_idx] & 0xFFFFF000);
    if (!(page_table[pt_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    return (page_table[pt_idx] & 0xFFFFF000) | offset;
}