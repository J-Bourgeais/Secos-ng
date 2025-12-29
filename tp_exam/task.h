#ifndef TASK_H
#define TASK_H

typedef struct {
    uint32_t kstack_ptr; // ESP dans la pile noyau
    uint32_t cr3;    // Registre CR3
} process_t;

extern process_t task1, task2;
extern process_t *current_task;
extern tss_t system_tss;
#endif
