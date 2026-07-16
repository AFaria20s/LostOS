#ifndef PAGING_H
#define PAGING_H

#include <stddef.h>
#include <stdint.h>

#define PAGING_PAGE_SIZE 4096

struct paging_stats {
  uintptr_t directory_addr;
  uintptr_t mapped_start;
  uintptr_t mapped_end;
  size_t mapped_bytes;
  size_t page_count;
  int enabled;
};

int paging_init(void);
int paging_is_enabled(void);
int paging_test(void);
void paging_get_stats(struct paging_stats *stats);

#endif
