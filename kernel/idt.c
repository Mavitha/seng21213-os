#include "../include/types.h"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIT_CH0    0x40
#define PIT_CMD    0x43
#define PIT_HZ     100         
#define PIT_DIVISOR (1193182 / PIT_HZ)


void pit_init(void) {
    
    outb(PIC1_CMD,  0x11); outb(PIC2_CMD,  0x11);
    outb(PIC1_DATA, 0x20); outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04); outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01); outb(PIC2_DATA, 0x01);
    outb(PIC1_DATA, 0xFE); outb(PIC2_DATA, 0xFF);

    
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(PIT_DIVISOR & 0xFF));
    outb(PIT_CH0, (uint8_t)(PIT_DIVISOR >> 8));
}

/* ---------------------------------------------------------------------------
 * IDT Structures and Initialization
 * --------------------------------------------------------------------------*/


struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;


extern void idt_load(uint32_t ptr);
extern void irq0_handler(void);


void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}


void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;

  
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    
    idt_set_gate(32, (uint32_t)irq0_handler, 0x08, 0x8E);


    idt_load((uint32_t)&idtp);
}