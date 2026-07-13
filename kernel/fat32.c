#include <stdint.h>

#include "../include/ata.h"
#include "../include/fat32.h"
#include "../include/mbr.h"

#define FAT32_SECTOR_SIZE 512
#define FAT32_DIRECTORY_ENTRY_SIZE 32
#define FAT32_ATTRIBUTE_DIRECTORY 0x10
#define FAT32_ATTRIBUTE_VOLUME_ID 0x08
#define FAT32_ATTRIBUTE_LONG_NAME 0x0F
#define FAT32_CLUSTER_END 0x0FFFFFF8

struct fat32_volume {
    uint32_t partition_lba;
    uint32_t fat_lba;
    uint32_t data_lba;
    uint32_t root_cluster;
    uint32_t sectors_per_fat;
    uint8_t sectors_per_cluster;
};

// Stores the mounted FAT32 volume
static struct fat32_volume volume;
static int fat32_ready;

// Reads a 16-bit little-endian value
static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

// Reads a 32-bit little-endian value
static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

// Reads one disk sector
static int fat32_read_sector(uint32_t lba, uint8_t *buffer) {
    return ata_read_sector(lba, (uint16_t *)buffer);
}

// Checks for the end of a cluster chain
static int fat32_is_end_cluster(uint32_t cluster) {
    return cluster >= FAT32_CLUSTER_END;
}

// Converts a cluster number to an LBA
static uint32_t fat32_cluster_lba(uint32_t cluster) {
    return volume.data_lba + (cluster - 2) * volume.sectors_per_cluster;
}

// Gets the next cluster from the FAT
static uint32_t fat32_next_cluster(uint32_t cluster) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t offset = cluster * 4;
    uint32_t lba = volume.fat_lba + offset / FAT32_SECTOR_SIZE;

    if (!fat32_read_sector(lba, sector))
        return FAT32_CLUSTER_END;

    return read_le32(sector + offset % FAT32_SECTOR_SIZE) & 0x0FFFFFFF;
}

// Checks if a directory entry can be used
static int fat32_entry_visible(const uint8_t *raw) {
    uint8_t attributes = raw[11];

    if (raw[0] == 0 || raw[0] == 0xE5)
        return 0;

    if (raw[0] == '.' || attributes == FAT32_ATTRIBUTE_LONG_NAME)
        return 0;

    return !(attributes & FAT32_ATTRIBUTE_VOLUME_ID);
}

// Parses an 8.3 directory entry
static void fat32_parse_entry(const uint8_t *raw, struct fat32_dirent *entry) {
    int length = 0;
    int has_extension = 0;

    for (int i = 0; i < 8 && raw[i] != ' '; i++)
        entry->name[length++] = raw[i];

    for (int i = 8; i < 11; i++) {
        if (raw[i] != ' ') {
            has_extension = 1;
            break;
        }
    }

    if (has_extension) {
        entry->name[length++] = '.';
        for (int i = 8; i < 11 && raw[i] != ' '; i++)
            entry->name[length++] = raw[i];
    }

    entry->name[length] = '\0';
    entry->attributes = raw[11];
    entry->first_cluster = ((uint32_t)read_le16(raw + 20) << 16) | read_le16(raw + 26);
    entry->size = read_le32(raw + 28);
}

// Compares a path name with an 8.3 entry
static int fat32_name_matches(const uint8_t *raw, const char *name, int length) {
    char expected[11];
    int extension = 0;
    int position = 0;

    for (int i = 0; i < 11; i++)
        expected[i] = ' ';

    for (int i = 0; i < length; i++) {
        char character = name[i];

        if (character == '.') {
            if (extension)
                return 0;
            extension = 1;
            position = 8;
            continue;
        }

        if (position >= 11 || (!extension && position >= 8))
            return 0;

        if (character >= 'a' && character <= 'z')
            character -= 'a' - 'A';

        expected[position++] = character;
    }

    for (int i = 0; i < 11; i++) {
        if (raw[i] != (uint8_t)expected[i])
            return 0;
    }

    return 1;
}

// Finds an entry inside a directory
static int fat32_find_entry(uint32_t cluster, const char *name, int length,
                            struct fat32_dirent *entry) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    while (!fat32_is_end_cluster(cluster) && cluster >= 2) {
        uint32_t lba = fat32_cluster_lba(cluster);

        for (int i = 0; i < volume.sectors_per_cluster; i++) {
            if (!fat32_read_sector(lba + i, sector))
                return 0;

            for (int j = 0; j < FAT32_SECTOR_SIZE; j += FAT32_DIRECTORY_ENTRY_SIZE) {
                if (sector[j] == 0)
                    return 0;

                if (fat32_entry_visible(sector + j) &&
                    fat32_name_matches(sector + j, name, length)) {
                    fat32_parse_entry(sector + j, entry);
                    return 1;
                }
            }
        }

        cluster = fat32_next_cluster(cluster);
    }

    return 0;
}

// Gets the cluster for a directory path
static int fat32_directory_cluster(const char *path, uint32_t *cluster) {
    struct fat32_file directory;

    if (!path || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        *cluster = volume.root_cluster;
        return 1;
    }

    if (!fat32_open(path, &directory) ||
        !(directory.attributes & FAT32_ATTRIBUTE_DIRECTORY))
        return 0;

    *cluster = directory.first_cluster;
    return 1;
}

int fat32_init(void) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    const struct mbr_partition *partition;
    uint16_t bytes_per_sector;
    uint16_t reserved_sectors;
    uint32_t sectors_per_fat;

    fat32_ready = 0;

    if (!mbr_init())
        return 0;

    partition = mbr_get_partition(0);
    if (!partition || (partition->type != 0x0B && partition->type != 0x0C))
        return 0;

    if (!fat32_read_sector(partition->lba_start, sector))
        return 0;

    bytes_per_sector = read_le16(sector + 11);
    reserved_sectors = read_le16(sector + 14);
    sectors_per_fat = read_le32(sector + 36);

    if (sector[510] != 0x55 || sector[511] != 0xAA ||
        bytes_per_sector != FAT32_SECTOR_SIZE || sector[13] == 0 ||
        sector[16] == 0 || reserved_sectors == 0 || sectors_per_fat == 0 ||
        read_le32(sector + 44) < 2)
        return 0;

    volume.partition_lba = partition->lba_start;
    volume.sectors_per_cluster = sector[13];
    volume.sectors_per_fat = sectors_per_fat;
    volume.fat_lba = partition->lba_start + reserved_sectors;
    volume.data_lba = volume.fat_lba + sector[16] * sectors_per_fat;
    volume.root_cluster = read_le32(sector + 44);
    fat32_ready = 1;
    return 1;
}

int fat32_open(const char *path, struct fat32_file *file) {
    struct fat32_dirent entry;
    uint32_t cluster;
    const char *part;

    if (!fat32_ready || !path || !file)
        return 0;

    cluster = volume.root_cluster;
    part = path;

    while (*part == '/')
        part++;

    if (*part == '\0') {
        file->first_cluster = cluster;
        file->size = 0;
        file->offset = 0;
        file->attributes = FAT32_ATTRIBUTE_DIRECTORY;
        return 1;
    }

    while (*part) {
        const char *next = part;
        int length;

        while (*next && *next != '/')
            next++;

        length = (int)(next - part);
        if (length == 0 || !fat32_find_entry(cluster, part, length, &entry))
            return 0;

        while (*next == '/')
            next++;

        if (*next == '\0') {
            file->first_cluster = entry.first_cluster;
            file->size = entry.size;
            file->offset = 0;
            file->attributes = entry.attributes;
            return 1;
        }

        if (!(entry.attributes & FAT32_ATTRIBUTE_DIRECTORY))
            return 0;

        cluster = entry.first_cluster;
        part = next;
    }

    return 0;
}

uint32_t fat32_read(struct fat32_file *file, void *buffer, uint32_t size) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint8_t *output = (uint8_t *)buffer;
    uint32_t read = 0;
    uint32_t cluster_size;

    if (!fat32_ready || !file || !buffer || file->offset >= file->size ||
        (file->attributes & FAT32_ATTRIBUTE_DIRECTORY))
        return 0;

    if (size > file->size - file->offset)
        size = file->size - file->offset;

    cluster_size = volume.sectors_per_cluster * FAT32_SECTOR_SIZE;

    while (read < size) {
        uint32_t cluster = file->first_cluster;
        uint32_t cluster_index = file->offset / cluster_size;
        uint32_t cluster_offset = file->offset % cluster_size;
        uint32_t sector_index = cluster_offset / FAT32_SECTOR_SIZE;
        uint32_t byte_offset = cluster_offset % FAT32_SECTOR_SIZE;
        uint32_t chunk = FAT32_SECTOR_SIZE - byte_offset;

        while (cluster_index-- > 0 && !fat32_is_end_cluster(cluster))
            cluster = fat32_next_cluster(cluster);

        if (cluster < 2 || fat32_is_end_cluster(cluster) ||
            !fat32_read_sector(fat32_cluster_lba(cluster) + sector_index, sector))
            break;

        if (chunk > size - read)
            chunk = size - read;

        for (uint32_t i = 0; i < chunk; i++)
            output[read + i] = sector[byte_offset + i];

        read += chunk;
        file->offset += chunk;
    }

    return read;
}

int fat32_readdir(const char *path, int index, struct fat32_dirent *entry) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t cluster;
    int current = 0;

    if (!fat32_ready || index < 0 || !entry ||
        !fat32_directory_cluster(path, &cluster))
        return 0;

    while (!fat32_is_end_cluster(cluster) && cluster >= 2) {
        uint32_t lba = fat32_cluster_lba(cluster);

        for (int i = 0; i < volume.sectors_per_cluster; i++) {
            if (!fat32_read_sector(lba + i, sector))
                return 0;

            for (int j = 0; j < FAT32_SECTOR_SIZE; j += FAT32_DIRECTORY_ENTRY_SIZE) {
                if (sector[j] == 0)
                    return 0;

                if (!fat32_entry_visible(sector + j))
                    continue;

                if (current++ == index) {
                    fat32_parse_entry(sector + j, entry);
                    return 1;
                }
            }
        }

        cluster = fat32_next_cluster(cluster);
    }

    return 0;
}
