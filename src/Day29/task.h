#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define KERNEL_STACK_SIZE 8192

typedef struct task_struct {
    uint32_t pid;
    uint32_t *stack;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
    uint32_t cr3;
    struct task_struct *next;
} task_t;

void task_init(void);
task_t *task_create(void (*entry)(void));
void schedule(void);

#endif