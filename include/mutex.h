#ifndef MUTEX_H
#define MUTEX_H

#include "types.h"

typedef struct {
    volatile int  locked;            
    int           owner;             
    int           waiters[16];        
    int           nwaiters;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif