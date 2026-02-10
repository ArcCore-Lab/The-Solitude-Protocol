#include "task.h"
#include "kmalloc.h"
#include <string.h>

static task_t *current_task = NULL;
static task_t *ready_queue = NULL;
static uint32_t next_pid = 0;

extern void switch_to(task_t *prev, task_t *next);

task_t *task_create(void (*entry)(void)) {
    task_t *task = (task_t *)kmalloc(sizeof(task_t));
    task->pid = next_pid++;
    task->stack = (uint32_t *)kmalloc(KERNEL_STACK_SIZE);
    task->esp = (uint32_t)(task->stack + KERNEL_STACK_SIZE / 4 - 1);
    task->cr3 = read_cr3();
    
    uint32_t *stack_top = (uint32_t *)task->esp;
    *(--stack_top) = (uint32_t)entry;  // EIP
    *(--stack_top) = 0;  // EBP
    *(--stack_top) = 0;  // EBX
    *(--stack_top) = 0;  // ESI
    *(--stack_top) = 0;  // EDI
    
    task->esp = (uint32_t)stack_top;
    task->next = NULL;
    
    if (!ready_queue) {
        ready_queue = task;
    } else {
        task_t *t = ready_queue;
        while (t->next) t = t->next;
        t->next = task;
    }
    
    return task;
}

void schedule(void) {
    if (!ready_queue) return;
    
    task_t *prev = current_task;
    current_task = ready_queue;
    ready_queue = ready_queue->next;
    current_task->next = NULL;
    
    if (prev) {
        task_t *t = ready_queue;
        if (!t) {
            ready_queue = prev;
        } else {
            while (t->next) t = t->next;
            t->next = prev;
        }
    }
    
    if (prev && prev != current_task) {
        switch_to(prev, current_task);
    }
}