#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

typedef struct multiboot_info {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;
  uint32_t syms[4];
  uint32_t mmap_length;
  uint32_t mmap_addr;
  uint32_t drives_length;
  uint32_t drives_addr;
  uint32_t config_table;
  uint32_t boot_loader_name;
  uint32_t apm_table;
  uint32_t vbe_control_info;
  uint32_t vbe_mode_info;
  uint16_t vbe_mode;
  uint16_t vbe_interface_seg;
  uint16_t vbe_interface_off;
  uint16_t vbe_interface_len;
  uint64_t framebuffer_addr;
  uint32_t framebuffer_pitch;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint8_t framebuffer_bpp;
  uint8_t framebuffer_type;
  uint16_t framebuffer_reserved;
} multiboot_info_t;

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
