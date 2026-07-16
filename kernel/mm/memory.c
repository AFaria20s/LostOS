#include <stddef.h>
#include <stdint.h>

#include "mm/memory.h"

#define HEAP_ALIGN 16
#define HEAP_MIN_SPLIT 32
#define HEAP_TOP_GUARD_BYTES (64 * 1024)

extern uint8_t kernel_start;
extern uint8_t kernel_end;

struct heap_block {
  size_t size;
  int free;
  struct heap_block *next;
  struct heap_block *prev;
};

static struct heap_block *heap_head = NULL;
static uintptr_t heap_start_addr = 0;
static uintptr_t heap_end_addr = 0;
static size_t total_memory_bytes = 0;
static size_t allocation_count = 0;
static int heap_ready = 0;

static uintptr_t align_up(uintptr_t value, size_t alignment) {
  return (value + alignment - 1) & ~((uintptr_t)alignment - 1);
}

static uintptr_t align_down(uintptr_t value, size_t alignment) {
  return value & ~((uintptr_t)alignment - 1);
}

static size_t align_size(size_t size) {
  return (size + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
}

static void split_block(struct heap_block *block, size_t size) {
  uintptr_t next_addr;
  struct heap_block *next;

  if (block->size < size + sizeof(struct heap_block) + HEAP_MIN_SPLIT)
    return;

  next_addr = (uintptr_t)(block + 1) + size;
  next = (struct heap_block *)next_addr;
  next->size = block->size - size - sizeof(struct heap_block);
  next->free = 1;
  next->next = block->next;
  next->prev = block;

  if (next->next)
    next->next->prev = next;

  block->size = size;
  block->next = next;
}

static void merge_with_next(struct heap_block *block) {
  struct heap_block *next = block->next;

  if (!next || !next->free)
    return;

  block->size += sizeof(struct heap_block) + next->size;
  block->next = next->next;

  if (block->next)
    block->next->prev = block;
}

void memory_init(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
  uintptr_t heap_start;
  uintptr_t heap_end;
  const struct multiboot_info *mbi;

  heap_ready = 0;
  heap_head = NULL;
  allocation_count = 0;
  total_memory_bytes = 0;

  if (multiboot_magic == MULTIBOOT_BOOTLOADER_MAGIC) {
    mbi = (const struct multiboot_info *)(uintptr_t)multiboot_info_addr;
    if (mbi && (mbi->flags & 0x1))
      total_memory_bytes = ((size_t)1024 + (size_t)mbi->mem_upper) * 1024;
  }

  if (total_memory_bytes == 0)
    total_memory_bytes = 16 * 1024 * 1024;

  heap_start = align_up((uintptr_t)&kernel_end, HEAP_ALIGN);
  heap_end = align_down((uintptr_t)total_memory_bytes, HEAP_ALIGN);

  if (heap_end > HEAP_TOP_GUARD_BYTES)
    heap_end -= HEAP_TOP_GUARD_BYTES;

  if (heap_end <= heap_start + sizeof(struct heap_block))
    return;

  heap_start_addr = heap_start;
  heap_end_addr = heap_end;

  heap_head = (struct heap_block *)heap_start_addr;
  heap_head->size = heap_end_addr - heap_start_addr - sizeof(struct heap_block);
  heap_head->free = 1;
  heap_head->next = NULL;
  heap_head->prev = NULL;
  heap_ready = 1;
}

void *kmalloc(size_t size) {
  struct heap_block *block;

  if (!heap_ready || size == 0)
    return NULL;

  size = align_size(size);

  for (block = heap_head; block; block = block->next) {
    if (!block->free || block->size < size)
      continue;

    split_block(block, size);
    block->free = 0;
    allocation_count++;
    return block + 1;
  }

  return NULL;
}

void kfree(void *ptr) {
  struct heap_block *block;

  if (!ptr)
    return;

  block = ((struct heap_block *)ptr) - 1;
  block->free = 1;
  if (allocation_count > 0)
    allocation_count--;

  merge_with_next(block);
  if (block->prev && block->prev->free)
    merge_with_next(block->prev);
}

void memory_get_stats(struct memory_stats *stats) {
  struct heap_block *block;
  size_t free_bytes = 0;

  if (!stats)
    return;

  stats->kernel_start = (uintptr_t)&kernel_start;
  stats->kernel_end = (uintptr_t)&kernel_end;
  stats->heap_start = heap_start_addr;
  stats->heap_end = heap_end_addr;
  stats->total_bytes = total_memory_bytes;
  stats->heap_bytes = heap_end_addr > heap_start_addr ? heap_end_addr - heap_start_addr : 0;
  stats->allocation_count = allocation_count;

  for (block = heap_head; block; block = block->next) {
    if (block->free)
      free_bytes += block->size;
  }

  stats->free_bytes = free_bytes;
  stats->used_bytes = stats->heap_bytes > free_bytes ? stats->heap_bytes - free_bytes : 0;
}

int memory_is_ready(void) {
  return heap_ready;
}
