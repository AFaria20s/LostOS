#include <stdint.h>
#include <stdio.h>

#define DISK_SECTORS 65536
#define SECTOR_SIZE 512
#define PARTITION_LBA 1
#define PARTITION_SECTORS (DISK_SECTORS - PARTITION_LBA)
#define RESERVED_SECTORS 32
#define FAT_SECTORS 512
#define FAT_COUNT 2
#define ROOT_CLUSTER 2

static void write_le16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static int write_sector(FILE *file, uint32_t lba, const uint8_t *sector) {
    if (fseek(file, (long)lba * SECTOR_SIZE, SEEK_SET) != 0)
        return 0;
    return fwrite(sector, 1, SECTOR_SIZE, file) == SECTOR_SIZE;
}

int main(void) {
    uint8_t sector[SECTOR_SIZE] = {0};
    FILE *file = fopen("disk.img", "wb");
    uint32_t fat_lba = PARTITION_LBA + RESERVED_SECTORS;

    if (!file)
        return 1;

    // grow the file to its final size
    if (fseek(file, (long)DISK_SECTORS * SECTOR_SIZE - 1, SEEK_SET) != 0 ||
        fputc(0, file) == EOF)
        return 1;

    // MBR with a single FAT32 partition
    sector[446] = 0x00;
    sector[450] = 0x0C;
    write_le32(sector + 454, PARTITION_LBA);
    write_le32(sector + 458, PARTITION_SECTORS);
    sector[510] = 0x55;
    sector[511] = 0xAA;
    if (!write_sector(file, 0, sector))
        return 1;

    // FAT32 boot sector (BPB)
    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    sector[0] = 0xEB;
    sector[1] = 0x58;
    sector[2] = 0x90;
    write_le16(sector + 11, SECTOR_SIZE);
    sector[13] = 1;
    write_le16(sector + 14, RESERVED_SECTORS);
    sector[16] = FAT_COUNT;
    sector[21] = 0xF8;
    write_le32(sector + 28, PARTITION_LBA);
    write_le32(sector + 32, PARTITION_SECTORS);
    write_le32(sector + 36, FAT_SECTORS);
    write_le32(sector + 44, ROOT_CLUSTER);
    write_le16(sector + 48, 1);
    write_le16(sector + 50, 6);
    sector[66] = 0x29;
    write_le32(sector + 67, 0x19400001);
    sector[510] = 0x55;
    sector[511] = 0xAA;
    if (!write_sector(file, PARTITION_LBA, sector))
        return 1;

    // FSInfo sector
    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    sector[0] = 0x52;
    sector[1] = 0x52;
    sector[2] = 0x61;
    sector[3] = 0x41;
    sector[484] = 0x72;
    sector[485] = 0x72;
    sector[486] = 0x41;
    sector[487] = 0x61;
    sector[510] = 0x55;
    sector[511] = 0xAA;
    if (!write_sector(file, PARTITION_LBA + 1, sector))
        return 1;

    // FAT tables, root cluster marked as end of chain, nothing else
    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    write_le32(sector, 0x0FFFFFF8);
    write_le32(sector + 4, 0x0FFFFFFF);
    write_le32(sector + 8, 0x0FFFFFFF);
    if (!write_sector(file, fat_lba, sector) ||
        !write_sector(file, fat_lba + FAT_SECTORS, sector))
        return 1;

    // root directory left empty
    // first_boot_setup() will populate it at kernel runtime

    fclose(file);
    return 0;
}