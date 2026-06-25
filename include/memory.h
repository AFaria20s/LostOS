#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

struct multiboot_info {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
};

struct memory_stats {
  uintptr_t kernel_start; 
  uintptr_t kernel_end;
  uintptr_t heap_start;
  uintptr_t heap_end;
  size_t total_bytes;
  size_t heap_bytes;
  size_t used_bytes;
  size_t free_bytes;
  size_t allocation_count;
};

void memory_init(uint32_t multiboot_magic, uint32_t multiboot_info_addr);
void *kmalloc(size_t size);
void kfree(void *ptr);
void memory_get_stats(struct memory_stats *stats);
int memory_is_ready(void);

#endif
