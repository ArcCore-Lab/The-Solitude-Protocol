#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PDE_PRESENT 0x001
#define PDE_RW 0x002
#define PDE_USER 0x004
#define PDE_SIZE 0x080

#define PTE_PRESENT 0x001
#define PTE_RW 0x002
#define PTE_USER 0x004

typedef uint32_t pde_t;
typedef uint32_t pte_t;

void paging_init(void);
void map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);
void unmap_page(uint32_t vaddr);
uint32_t get_physical_address(uint32_t vaddr);

#endif