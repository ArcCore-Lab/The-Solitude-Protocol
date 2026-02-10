#ifndef GDT_IDT_H
#define GDT_IDT_H

#include <stdint.h>

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint16_t zero;
    uint16_t type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((pack)) idt_descriptor_t;

#define IDT_ENTRIES 256

void idt_init(void);
void idt_set_get(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);


#endif