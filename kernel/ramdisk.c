#include "../include/ramdisk.h"

static uint8_t disk_memory[RAMDISK_SIZE];

static void rd_memcpy(void *dst, const void *src, uint32_t count) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (count--) *d++ = *s++;
}

void ramdisk_init(void) {
   
    uint8_t *d = (uint8_t *)disk_memory;
    for (uint32_t i = 0; i < RAMDISK_SIZE; i++) d[i] = 0;
}

void ramdisk_read(uint32_t block, void *buf) {
    uint32_t offset = block * BLOCK_SIZE;
    if (offset + BLOCK_SIZE <= RAMDISK_SIZE) {
        rd_memcpy(buf, &disk_memory[offset], BLOCK_SIZE);
    }
}

void ramdisk_write(uint32_t block, const void *buf) {
    uint32_t offset = block * BLOCK_SIZE;
    if (offset + BLOCK_SIZE <= RAMDISK_SIZE) {
        rd_memcpy(&disk_memory[offset], buf, BLOCK_SIZE);
    }
}