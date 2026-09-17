#ifndef PMM_H
#define PMM_H

#include "types.h"

typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_ext;
} __attribute__((packed)) e820_entry_t;

void pmm_init(void);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t phys);
uint32_t pmm_free_frames(void);
uint32_t pmm_total_frames(void);

#endif 