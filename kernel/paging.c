#include <stddef.h>
#include <stdint.h>

#include "../include/paging.h"

#define PAGE_PRESENT 0x001
#define PAGE_WRITABLE 0x002
#define PAGE_TABLE_COUNT 64
#define PAGES_PER_TABLE 1024
#define PAGING_MAPPED_BYTES (PAGE_TABLE_COUNT * PAGES_PER_TABLE * PAGING_PAGE_SIZE)

static uint32_t page_directory[1024] __attribute__((aligned(PAGING_PAGE_SIZE)));
static uint32_t page_tables[PAGE_TABLE_COUNT][PAGES_PER_TABLE] __attribute__((aligned(PAGING_PAGE_SIZE)));

static int paging_ready = 0;

static uintptr_t read_cr0(void) {
  uintptr_t value;
  __asm__ volatile("mov %%cr0, %0" : "=r"(value));
  return value;
}

static uintptr_t read_cr3(void) {
  uintptr_t value;
  __asm__ volatile("mov %%cr3, %0" : "=r"(value));
  return value;
}

static void write_cr0(uintptr_t value) {
  __asm__ volatile("mov %0, %%cr0" : : "r"(value) : "memory");
}

static void write_cr3(uintptr_t value) {
  __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

int paging_init(void) {
  for (size_t i = 0; i < 1024; i++)
    page_directory[i] = PAGE_WRITABLE;

  for (size_t table = 0; table < PAGE_TABLE_COUNT; table++) {
    for (size_t page = 0; page < PAGES_PER_TABLE; page++) {
      uintptr_t addr = (table * PAGES_PER_TABLE + page) * PAGING_PAGE_SIZE;
      page_tables[table][page] = (uint32_t)(addr | PAGE_PRESENT | PAGE_WRITABLE);
    }

    page_directory[table] = (uint32_t)((uintptr_t)page_tables[table] |
                                       PAGE_PRESENT | PAGE_WRITABLE);
  }

  write_cr3((uintptr_t)page_directory);
  write_cr0(read_cr0() | 0x80000000);

  paging_ready = paging_is_enabled() &&
                 (read_cr3() & 0xFFFFF000) == (uintptr_t)page_directory;

  return paging_ready;
}

int paging_is_enabled(void) {
  return (read_cr0() & 0x80000000) != 0;
}

int paging_test(void) {
  volatile uint32_t probe = 0x19401940;

  if (!paging_is_enabled())
    return 0;

  probe ^= 0xFFFFFFFF;
  return probe == 0xE6BFE6BF;
}

void paging_get_stats(struct paging_stats *stats) {
  if (!stats)
    return;

  stats->directory_addr = (uintptr_t)page_directory;
  stats->mapped_start = 0;
  stats->mapped_end = PAGING_MAPPED_BYTES;
  stats->mapped_bytes = PAGING_MAPPED_BYTES;
  stats->page_count = PAGE_TABLE_COUNT * PAGES_PER_TABLE;
  stats->enabled = paging_ready && paging_is_enabled();
}
