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

// Initialize and identify the primary master ATA drive.
// Returns 1 if a drive was found, 0 otherwise.
int ata_identify(struct ata_info *info);

#endif