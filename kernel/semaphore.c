#include "../include/semaphore.h"
#include "../include/types.h"
#include "../include/process.h" 

extern int current_proc; 

void sem_init(semaphore_t *s, int initial) {
    s->count = initial;
    s->nwaiters = 0;
    for (int i = 0; i < 16; i++) {
        s->waiters[i] = 0;
    }
}

void sem_wait(semaphore_t *s) {
    s->count--;
    if (s->count < 0) {
    
        s->waiters[s->nwaiters++] = current_proc;
        proc_table[current_proc].state = PROC_BLOCKED;
        
        
        __asm__ volatile("int $0x20"); 
    }
}

void sem_signal(semaphore_t *s) {
    s->count++;
    
    if (s->count <= 0 && s->nwaiters > 0) {
        int target = s->waiters[--s->nwaiters];
        proc_table[target].state = PROC_READY;
    }
}