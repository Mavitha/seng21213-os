#include "../include/process.h"

pcb_t proc_table[MAX_PROCS];
int   current_proc = -1;
uint32_t next_pid = 1;

// static void k_strcpy(char *dest, const char *src) {
//     while (*src) {
//         *dest++ = *src++;
//     }
//     *dest = '\0';
// }

pcb_t* proc_create(const char *name, void (*entry)(void)) {
    pcb_t *p = 0;
    int pid = -1;


    for (int i = 0; i < MAX_PROCS; i++) {
        if (proc_table[i].state == 0) { 
            p = &proc_table[i];
            pid = i;
            break;
        }
    }



    if (p == 0) return 0; 

    p->pid = pid;
    p->state = 1; 
    p->ticks = 0;

    int j = 0;
    while (name[j] != '\0' && j < 8) {
        p->name[j] = name[j];
        j++;
    }
    p->name[j] = '\0';


    p->stack_base = 0x400000 + (pid * 4096);
    p->esp = p->stack_base;

    uint32_t *stack = (uint32_t *)p->esp;
    *(--stack) = (uint32_t)entry;
    
    
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 

    p->esp = (uint32_t)stack; 
    return p;
}

void proc_exit(void) {
    if (current_proc >= 0 && current_proc < MAX_PROCS) {
        proc_table[current_proc].state = PROC_UNUSED;
    }
}

