#ifndef THREAD_H
#define THREAD_H

#include "types.h"
#include "process.h" 

typedef struct {
    uint32_t  tid;
    uint32_t  pid;            
    uint32_t  esp;          
    uint8_t   stack[STACK_SIZE];
    proc_state_t state;
    char      name[24];
} tcb_t;

tcb_t *thread_create(uint32_t pid, const char *name, void (*fn)(void));
void   thread_exit(void);

#endif