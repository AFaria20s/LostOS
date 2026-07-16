#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// ATA drive status
typedef enum {
    ATA_NONE = 0,      // no drive detected
    ATA_READY,         // drive ready
    ATA_ERROR          // drive error
} ata_status_t;

// ATA drive info
struct ata_info {
    ata_status_t status;
    char model[41];        // drive model string (40 chars + null)
    uint32_t lba28_sectors; // total sectors (LBA28)
    uint64_t size_mb;       // size in MB
};

// Initialize and identify the primary master ATA drive
// returns 1 if a drive was found and 0 if not
int ata_identify(struct ata_info *info);
// Reads a disk sector and store it in buffer
// returns 1 if success
int ata_read_sector(uint32_t lba, uint16_t *buf);
// Writes a disk sector from buf
// returns 1 if success
int ata_write_sector(uint32_t lba, uint16_t *buf);

int ata_init(void);
int ata_is_ready(void);

const struct ata_info *ata_get_info(void);

#endif