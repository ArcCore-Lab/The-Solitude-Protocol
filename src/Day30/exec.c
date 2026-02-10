#include <stdint.h>
#include <string.h>
#include "task.h"
#include "kmalloc.h"
#include "paging.h"

#define USER_CODE_START 0x08048000
#define USER_STACK_TOP  0xC0000000
#define USER_STACK_SIZE 0x2000

extern task_t *current_task;
extern void printf(const char *fmt, ...);
extern char *gets(char *buf, int max_len);

extern uint8_t _binary_user_program_start[];
extern uint8_t _binary_user_program_end[];

int32_t sys_exec(const char *path) {
    if (!current_task) return -1;
    
    uint32_t program_size = (uint32_t)_binary_user_program_end - 
                           (uint32_t)_binary_user_program_start;
    
    uint32_t pages_needed = (program_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < pages_needed; i++) {
        uint32_t paddr = pmm_alloc_page();
        if (!paddr) return -1;
        
        map_page(USER_CODE_START + i * PAGE_SIZE, paddr, 
                 PTE_PRESENT | PTE_RW | PTE_USER);
    }
    
    memcpy((void *)USER_CODE_START, _binary_user_program_start, program_size);
    
    for (uint32_t i = 0; i < USER_STACK_SIZE / PAGE_SIZE; i++) {
        uint32_t paddr = pmm_alloc_page();
        if (!paddr) return -1;
        
        map_page(USER_STACK_TOP - USER_STACK_SIZE + i * PAGE_SIZE, 
                 paddr, PTE_PRESENT | PTE_RW | PTE_USER);
    }
    
    __asm__ volatile (
        "cli\n"              
        "mov $0x23, %%ax\n"  
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        

        "pushl $0x23\n"            // SS
        "pushl %0\n"               // ESP
        "pushfl\n"                 // EFLAGS
        "popl %%eax\n"
        "orl $0x200, %%eax\n"      // IF
        "pushl %%eax\n"
        "pushl $0x1B\n"            // CS
        "pushl %1\n"               // EIP 
        "iret\n"
        : 
        : "r"(USER_STACK_TOP), "r"(USER_CODE_START)
        : "eax"
    );
    
    return 0;
}

extern void printf(const char *fmt, ...);
extern int32_t sys_exec(const char *path);

void shell_main(void) {
    char buffer[128];
    
    printf("ArcCore Shell v0.1\n");
    printf("Type 'help' for available commands\n\n");
    
    while (1) {
        printf("$ ");
        
        if (gets(buffer, sizeof(buffer) - 1) == NULL) {
            continue;
        }
        
        buffer[strcspn(buffer, "\n")] = '\0';
        
        if (strlen(buffer) == 0) {
            continue;
        }
        
        if (strcmp(buffer, "help") == 0) {
            printf("Available commands:\n");
            printf("  help  - Show this message\n");
            printf("  exec  - Execute a program\n");
            printf("  exit  - Exit shell\n");
        } 
        else if (strncmp(buffer, "exec ", 5) == 0) {
            char *path = buffer + 5;
            printf("Executing: %s\n", path);
            sys_exec(path);
        }
        else if (strcmp(buffer, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        } 
        else {
            printf("Unknown command: %s\n", buffer);
            printf("Type 'help' for available commands\n");
        }
    }
}