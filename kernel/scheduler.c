#include "process.h"
#include "types.h"


extern void context_switch(uint32_t *old_esp, uint32_t new_esp);

void scheduler_tick(void) {
  
    proc_table[current_proc].ticks++;

  
    int next = (current_proc + 1) % MAX_PROCS;
    int searched = 0;
    while (searched < MAX_PROCS) {
        if (proc_table[next].state == PROC_READY) break;
        next = (next + 1) % MAX_PROCS;
        searched++;
    }

    if (searched == MAX_PROCS) {
        outb(0x20, 0x20); 
        return; 
    }

    
    int old = current_proc;
    proc_table[old].state = PROC_READY;
    proc_table[next].state = PROC_RUNNING;
    current_proc = next;

    
    outb(0x20, 0x20);

   
    context_switch(&proc_table[old].esp, proc_table[next].esp);
}