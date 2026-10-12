#include <stddef.h>
#include <stdint.h>

#include "mm/paging.h"

#define PAGE_TABLE_COUNT 64
#define PAGE_TABLE_POOL_COUNT 1024
#define PAGES_PER_TABLE 1024
#define PAGING_MAPPED_BYTES (PAGE_TABLE_COUNT * PAGES_PER_TABLE * PAGING_PAGE_SIZE)

static uint32_t page_directory[1024] __attribute__((aligned(PAGING_PAGE_SIZE)));
static uint32_t page_tables[PAGE_TABLE_POOL_COUNT][PAGES_PER_TABLE]
  __attribute__((aligned(PAGING_PAGE_SIZE)));

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

static void flush_page(uintptr_t address) {
  __asm__ volatile("invlpg (%0)" : : "r"(address) : "memory");
}

int paging_init(void) {
  for (size_t i = 0; i < 1024; i++)
    page_directory[i] = PAGING_PAGE_WRITABLE;

  for (size_t table = 0; table < PAGE_TABLE_COUNT; table++) {
    for (size_t page = 0; page < PAGES_PER_TABLE; page++) {
      uintptr_t addr = (table * PAGES_PER_TABLE + page) * PAGING_PAGE_SIZE;
      page_tables[table][page] = (uint32_t)(addr | PAGING_PAGE_PRESENT |
                                            PAGING_PAGE_WRITABLE);
    }

    page_directory[table] = (uint32_t)((uintptr_t)page_tables[table] |
                                       PAGING_PAGE_PRESENT |
                                       PAGING_PAGE_WRITABLE);
  }

  write_cr3((uintptr_t)page_directory);
  write_cr0(read_cr0() | 0x80000000);

  paging_ready = paging_is_enabled() &&
                 (read_cr3() & 0xFFFFF000) == (uintptr_t)page_directory;

  return paging_ready;
}

int paging_map_range(uint32_t phys, uint32_t size, uint32_t flags) {
  uint64_t range_end;
  uint64_t start;
  uint64_t end;
  uint64_t address;

  if (!paging_ready || size == 0)
    return -1;

  range_end = (uint64_t)phys + size;
  if (range_end > 0x100000000ULL)
    return -1;

  start = phys & ~(PAGING_PAGE_SIZE - 1);
  end = (range_end + PAGING_PAGE_SIZE - 1) &
        ~(uint64_t)(PAGING_PAGE_SIZE - 1);

  for (address = start; address < end; address += PAGING_PAGE_SIZE) {
    uint32_t directory_index = (uint32_t)(address >> 22);
    uint32_t table_index = (uint32_t)((address >> 12) & 0x3FF);

    if (!(page_directory[directory_index] & PAGING_PAGE_PRESENT)) {
      for (size_t page = 0; page < PAGES_PER_TABLE; page++)
        page_tables[directory_index][page] = 0;

      page_directory[directory_index] =
        (uint32_t)((uintptr_t)page_tables[directory_index] |
                   PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE);
    }

    page_tables[directory_index][table_index] = (uint32_t)address | flags;
    flush_page((uintptr_t)address);
  }

  return 0;
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
