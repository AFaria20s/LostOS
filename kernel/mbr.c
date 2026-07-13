#include <stddef.h>
#include <stdint.h>

#include "../include/ata.h"
#include "../include/mbr.h"

// MBR partition table layout
#define MBR_PARTITION_TABLE_OFFSET  0x1BE
#define MBR_PARTITION_ENTRY_SIZE    16

// Parsed partition cache
static struct mbr_partition partitions[MBR_PARTITION_COUNT];
static int mbr_valid;

// Reads a 32 bit little-endian value
static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

int mbr_init(void) {
    uint16_t sector[256];
    uint8_t *bytes = (uint8_t *)sector;

    mbr_valid = 0;

    if (!ata_read_sector(0, sector))
        return 0;

    // Validate the MBR signature
    if (bytes[510] != 0x55 || bytes[511] != 0xAA)
        return 0;

    for (int i = 0; i < MBR_PARTITION_COUNT; i++) {
        const uint8_t *entry = bytes + MBR_PARTITION_TABLE_OFFSET + i * MBR_PARTITION_ENTRY_SIZE;

        partitions[i].status = entry[0];
        partitions[i].type = entry[4];
        partitions[i].lba_start = read_le32(entry + 8);
        partitions[i].sector_count = read_le32(entry + 12);
    }

    mbr_valid = 1;
    return 1;
}

const struct mbr_partition *mbr_get_partition(int index) {
    if (!mbr_valid || index < 0 || index >= MBR_PARTITION_COUNT)
        return NULL;

    return &partitions[index];
}
