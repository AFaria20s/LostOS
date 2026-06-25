#include "../include/ata.h"
#include "../include/io.h"

// ATA primary bus ports
#define ATA_PORT_DATA        0x1F0
#define ATA_PORT_ERROR       0x1F1
#define ATA_PORT_SECTOR_CNT  0x1F2
#define ATA_PORT_LBA_LOW     0x1F3
#define ATA_PORT_LBA_MID     0x1F4
#define ATA_PORT_LBA_HIGH    0x1F5
#define ATA_PORT_DRIVE       0x1F6
#define ATA_PORT_CMD         0x1F7
#define ATA_PORT_STATUS      0x1F7

// ATA status bits
#define ATA_STATUS_ERR  0x01  // error
#define ATA_STATUS_DRQ  0x08  // data request ready
#define ATA_STATUS_BSY  0x80  // busy

// ATA commands
#define ATA_CMD_IDENTIFY 0xEC

static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// Wait for BSY to clear, returns 0 on timeout or error
static int ata_wait_ready(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_PORT_STATUS);
        if (status == 0xFF) return 0;  // floating bus, no drive
        if (status & ATA_STATUS_ERR)  return 0;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) return 1;
    }
    return 0;
}

// Copy and trim a string from the IDENTIFY buffer.
// ATA strings are byte-swapped: each pair of bytes is reversed.
static void ata_copy_string(uint16_t *buf, int word_start, int word_count, char *out) {
    int j = 0;
    for (int i = word_start; i < word_start + word_count; i++) {
        out[j++] = (char)(buf[i] >> 8);
        out[j++] = (char)(buf[i] & 0xFF);
    }
    out[j] = '\0';

    // trim trailing spaces
    for (int k = j - 1; k >= 0 && out[k] == ' '; k--)
        out[k] = '\0';
}

int ata_identify(struct ata_info *info) {
    uint16_t buf[256];

    info->status = ATA_NONE;
    info->model[0] = '\0';
    info->lba28_sectors = 0;
    info->size_mb = 0;

    // select primary master drive
    outb(ATA_PORT_DRIVE, 0xA0);

    // zero out sector count and LBA registers
    outb(ATA_PORT_SECTOR_CNT, 0);
    outb(ATA_PORT_LBA_LOW,    0);
    outb(ATA_PORT_LBA_MID,    0);
    outb(ATA_PORT_LBA_HIGH,   0);

    // send IDENTIFY command
    outb(ATA_PORT_CMD, ATA_CMD_IDENTIFY);

    // check if drive exists
    uint8_t status = inb(ATA_PORT_STATUS);
    if (status == 0x00 || status == 0xFF) {
        info->status = ATA_NONE;
        return 0;
    }

    // wait for drive to be ready
    if (!ata_wait_ready()) {
        info->status = ATA_ERROR;
        return 0;
    }

    // check mid/high LBA - se nao zero e ATAPI, nao ATA
    uint8_t lba_mid  = inb(ATA_PORT_LBA_MID);
    uint8_t lba_high = inb(ATA_PORT_LBA_HIGH);
    if (lba_mid != 0 || lba_high != 0) {
        info->status = ATA_NONE;
        return 0;
    }

    // read 256 words
    for (int i = 0; i < 256; i++)
        buf[i] = inw(ATA_PORT_DATA);

    // model string is in words 27-46
    ata_copy_string(buf, 27, 20, info->model);

    // LBA28 sector count is in words 60-61
    info->lba28_sectors = ((uint32_t)buf[61] << 16) | buf[60];
    info->size_mb = ((uint64_t)info->lba28_sectors * 512) / (1024 * 1024);

    info->status = ATA_READY;
    return 1;
}