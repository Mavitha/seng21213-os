#ifndef RAMDISK_H
#define RAMDISK_H

#include "types.h"

#define RAMDISK_SIZE (1024 * 1024)
#define BLOCK_SIZE   512

void ramdisk_init(void);
void ramdisk_read(uint32_t block, void *buf);
void ramdisk_write(uint32_t block, const void *buf);

#endif