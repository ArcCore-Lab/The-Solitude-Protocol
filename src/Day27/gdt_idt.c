#include "gdt_idt.h"
#include <string.h>
#include <stdint.h>

static idt_entry_t idt[IDT_ENTRIES];
static idt_descriptor_t idt_desc;

extern void idt_load(uint32_t);

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

void idt_init(void) {
    idt_desc.limit = sizeof(idt) - 1;
    idt_desc.base = (uint32_t)&idt;

    memset(&idt, 0, sizeof(idt));

    idt_load((uint32_t)&idt_desc);
}

#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define ICW1_INIT    0x11
#define ICW4_8086    0x01

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    outb(PIC1_COMMAND, ICW1_INIT);
    outb(PIC2_COMMAND, ICW1_INIT);
    
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
    
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);
    }
    outb(PIC1_COMMAND, 0x20);
}

extern void irq0(void);
extern void irq1(void);

static volatile uint32_t timer_ticks = 0;

static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

void irq_handler(uint32_t irq_num) {
    if (irq_num == 0) {
        timer_ticks++;
        if (timer_ticks % 10 == 0) { 
            schedule();
        }
    } else if (irq_num == 1) {
        uint8_t scancode = inb(0x60);
        
        if (scancode < 58 && !(scancode & 0x80)) {
            char c = scancode_to_ascii[scancode];
            if (c) {
                putchar(c);
            }
        }
    }
    
    pic_send_eoi(irq_num);
}

void interrupts_init(void) {
    pic_remap();
    
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E); 
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    
    __asm__ volatile ("sti");
}

uint32_t get_timer_ticks(void) {
    return timer_ticks;
}

void kernel_main(void) {
    
    idt_init();
    interrupts_init();
    
    printf("Interrupts enabled. Press keys to test!\n");
    
    while (1) {
        __asm__ volatile ("hlt");
    }
}