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

// Writes a 16-bit little-endian value
static void write_le16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

// Writes a 32-bit little-endian value
static void write_le32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

// Writes one sector to the disk image
static int write_sector(FILE *file, uint32_t lba, const uint8_t *sector) {
    if (fseek(file, (long)lba * SECTOR_SIZE, SEEK_SET) != 0)
        return 0;

    return fwrite(sector, 1, SECTOR_SIZE, file) == SECTOR_SIZE;
}

// Writes an 8.3 FAT name
static void write_name(uint8_t *entry, const char *base, const char *extension) {
    for (int i = 0; i < 11; i++)
        entry[i] = ' ';

    for (int i = 0; base[i] != '\0' && i < 8; i++)
        entry[i] = (uint8_t)base[i];

    for (int i = 0; extension[i] != '\0' && i < 3; i++)
        entry[8 + i] = (uint8_t)extension[i];
}

int main(void) {
    uint8_t sector[SECTOR_SIZE] = {0};
    FILE *file = fopen("disk.img", "wb");
    uint32_t fat_lba = PARTITION_LBA + RESERVED_SECTORS;
    uint32_t data_lba = fat_lba + FAT_COUNT * FAT_SECTORS;
    const char text[] = "Hello from the FAT32 disk.\n";
    const char info[] = "Read from a FAT32 subdirectory.\n";

    if (!file)
        return 1;

    if (fseek(file, (long)DISK_SECTORS * SECTOR_SIZE - 1, SEEK_SET) != 0 ||
        fputc(0, file) == EOF)
        return 1;

    sector[446] = 0x00;
    sector[450] = 0x0C;
    write_le32(sector + 454, PARTITION_LBA);
    write_le32(sector + 458, PARTITION_SECTORS);
    sector[510] = 0x55;
    sector[511] = 0xAA;
    if (!write_sector(file, 0, sector))
        return 1;

    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    sector[0] = 0xEB;
    sector[1] = 0x58;
    sector[2] = 0x90;
    sector[3] = '0';
    sector[4] = 'x';
    sector[5] = '1';
    sector[6] = '9';
    sector[7] = '4';
    sector[8] = 'O';
    sector[9] = 'S';
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
    sector[64] = 0x80;
    sector[66] = 0x29;
    write_le32(sector + 67, 0x19400001);
    sector[71] = '0';
    sector[72] = 'X';
    sector[73] = '1';
    sector[74] = '9';
    sector[75] = '4';
    sector[76] = 'O';
    sector[77] = 'S';
    sector[82] = 'F';
    sector[83] = 'A';
    sector[84] = 'T';
    sector[85] = '3';
    sector[86] = '2';
    sector[510] = 0x55;
    sector[511] = 0xAA;
    if (!write_sector(file, PARTITION_LBA, sector))
        return 1;

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

    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    write_le32(sector, 0x0FFFFFF8);
    write_le32(sector + 4, 0x0FFFFFFF);
    write_le32(sector + 8, 0x0FFFFFFF);
    write_le32(sector + 12, 0x0FFFFFFF);
    write_le32(sector + 16, 0x0FFFFFFF);
    write_le32(sector + 20, 0x0FFFFFFF);
    if (!write_sector(file, fat_lba, sector) ||
        !write_sector(file, fat_lba + FAT_SECTORS, sector))
        return 1;

    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    write_name(sector, "HELLO", "TXT");
    sector[11] = 0x20;
    write_le16(sector + 26, 3);
    write_le32(sector + 28, sizeof(text) - 1);
    write_name(sector + 32, "DOCS", "");
    sector[43] = 0x10;
    write_le16(sector + 58, 4);
    if (!write_sector(file, data_lba, sector))
        return 1;

    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    for (int i = 0; i < (int)sizeof(text) - 1; i++)
        sector[i] = (uint8_t)text[i];
    if (!write_sector(file, data_lba + 1, sector))
        return 1;

    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    write_name(sector, "INFO", "TXT");
    sector[11] = 0x20;
    write_le16(sector + 26, 5);
    write_le32(sector + 28, sizeof(info) - 1);
    if (!write_sector(file, data_lba + 2, sector))
        return 1;

    for (int i = 0; i < SECTOR_SIZE; i++)
        sector[i] = 0;
    for (int i = 0; i < (int)sizeof(info) - 1; i++)
        sector[i] = (uint8_t)info[i];
    if (!write_sector(file, data_lba + 3, sector))
        return 1;

    fclose(file);
    return 0;
}
