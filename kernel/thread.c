#include "../include/thread.h"
#include "../include/types.h"

#define MAX_THREADS 16
tcb_t thread_table[MAX_THREADS];
int next_tid = 1;

tcb_t *thread_create(uint32_t pid, const char *name, void (*fn)(void)) {
    tcb_t *t = 0;
    int tid = -1;


    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state == 0) { 
            t = &thread_table[i];
            tid = i;
            break;
        }
    }

    if (t == 0) return 0; 

    t->tid = tid;
    t->pid = pid;
    t->state = 1; 

   
    int j = 0;
    while (name[j] != '\0' && j < 23) {
        t->name[j] = name[j];
        j++;
    }
    t->name[j] = '\0';

    uint32_t stack_top = (uint32_t)&t->stack[STACK_SIZE];
    uint32_t *stack = (uint32_t *)stack_top;

    *(--stack) = (uint32_t)fn; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0; 
    *(--stack) = 0;

    t->esp = (uint32_t)stack; 
    return t;
}

void thread_exit(void) {
}