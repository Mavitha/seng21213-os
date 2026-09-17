#include "pmm.h"

#define FRAME_SIZE      4096
#define BITMAP_SIZE     (32 * 1024 * 1024 / FRAME_SIZE / 32)


extern uint32_t _end;
#define KERNEL_END ((uint32_t)&_end)

static uint32_t bitmap[BITMAP_SIZE]; 
static uint32_t total_frames = 0;
static uint32_t free_frames = 0;

static void k_memset(void *dest, uint8_t val, uint32_t count) {
    uint8_t *temp = (uint8_t *)dest;
    for (; count != 0; count--) *temp++ = val;
}

static inline void bitmap_set  (uint32_t f) { bitmap[f/32] |=  (1u << (f%32)); }
static inline void bitmap_clear(uint32_t f) { bitmap[f/32] &= ~(1u << (f%32)); }
static inline int  bitmap_test (uint32_t f) { return (bitmap[f/32] >> (f%32)) & 1; }

void pmm_init(void) {
   
    k_memset(bitmap, 0xFF, sizeof(bitmap));

    uint16_t count = *(uint16_t *)0x8000;
    e820_entry_t *map = (e820_entry_t *)0x8004;

    for (int i = 0; i < count; i++) {
        if (map[i].type != 1) continue;  
        
        uint32_t start = (uint32_t)map[i].base;
        uint32_t len   = (uint32_t)map[i].length;
        
        uint32_t first = (start < KERNEL_END) ? KERNEL_END : start; 

        if (first % FRAME_SIZE != 0) {
            first = (first + FRAME_SIZE) & ~(FRAME_SIZE - 1);
        }

        for (uint32_t a = first; a < start + len; a += FRAME_SIZE) {
            bitmap_clear(a / FRAME_SIZE);
            free_frames++;
        }
    }
    total_frames = (32 * 1024 * 1024) / FRAME_SIZE; 
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_frames--;
            return i * FRAME_SIZE; 
        }
    }
    return 0;  
}

void pmm_free_frame(uint32_t phys) {
    uint32_t frame = phys / FRAME_SIZE;
    bitmap_clear(frame);
    free_frames++;
}

uint32_t pmm_free_frames (void)  { return free_frames;  }
uint32_t pmm_total_frames(void) { return total_frames; }