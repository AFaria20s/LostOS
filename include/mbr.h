#ifndef MBR_H
#define MBR_H

#include <stdint.h>

#define MBR_PARTITION_COUNT 4

struct mbr_partition {
    uint8_t status;
    uint8_t type;
    uint32_t lba_start;
    uint32_t sector_count;
};

int mbr_init(void);
const struct mbr_partition *mbr_get_partition(int index);

#endif
