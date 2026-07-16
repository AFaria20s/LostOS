#include <stdint.h>

#include "drivers/ata.h"
#include "fs/fat32.h"
#include "lib/kstring.h"
#include "fs/mbr.h"

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

static int fat32_read_sector(uint32_t lba, uint8_t *buffer);

static uint32_t fat32_alloc_cluster(void) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    
    // go through all FAT sectors
    for (uint32_t i = 0; i < volume.sectors_per_fat; i++) {
        if (!fat32_read_sector(volume.fat_lba + i, sector))
            return 0;
        
        // each sector has 128 4 byte entries
        for (int j = 0; j < 128; j++) {
            uint32_t value = read_le32(sector + j * 4);
            
            if (value == 0x00000000) {
                // free cluster found, set as chain end
                uint32_t cluster = i * 128 + j;
                if (cluster < 2) continue;
                
                // write 0x0FFFFFFF on the entry
                sector[j * 4 + 0] = 0xFF;
                sector[j * 4 + 1] = 0xFF;
                sector[j * 4 + 2] = 0xFF;
                sector[j * 4 + 3] = 0x0F;
                
                ata_write_sector(volume.fat_lba + i, (uint16_t *)sector);
                // write in FAT backup
                ata_write_sector(volume.fat_lba + volume.sectors_per_fat + i, (uint16_t *)sector);
                
                return cluster;
            }
        }
    }
    
    return 0;
}

// Reads one disk sector
static int fat32_read_sector(uint32_t lba, uint8_t *buffer) {
    return ata_read_sector(lba, (uint16_t *)buffer);
}

// Writes one disk sector
static int fat32_write_sector(uint32_t lba, uint8_t *buffer) {
    return ata_write_sector(lba, (uint16_t *)buffer);
}

// Links a cluster to the next one in the chain, updates both FAT copies
static int fat32_set_next_cluster(uint32_t cluster, uint32_t value) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint32_t offset = cluster * 4;
    uint32_t lba = volume.fat_lba + offset / FAT32_SECTOR_SIZE;
    uint32_t index = offset % FAT32_SECTOR_SIZE;

    if (!fat32_read_sector(lba, sector))
        return 0;

    sector[index + 0] = value & 0xFF;
    sector[index + 1] = (value >> 8) & 0xFF;
    sector[index + 2] = (value >> 16) & 0xFF;
    sector[index + 3] = (value >> 24) & 0x0F;

    fat32_write_sector(lba, sector);
    fat32_write_sector(lba + volume.sectors_per_fat, sector);
    return 1;
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

// Formats a name into an 11-byte 8.3 slot (uppercase, space padded)
// Returns 0 if the name doesn't fit the 8.3 pattern
static int fat32_format_name(const char *name, int length, uint8_t *raw) {
    int extension = 0;
    int position = 0;

    for (int i = 0; i < 11; i++)
        raw[i] = ' ';

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

        raw[position++] = (uint8_t)character;
    }

    return 1;
}

// Compares a path name with an 8.3 entry
static int fat32_name_matches(const uint8_t *raw, const char *name, int length) {
    uint8_t expected[11];

    if (!fat32_format_name(name, length, expected))
        return 0;

    for (int i = 0; i < 11; i++) {
        if (raw[i] != expected[i])
            return 0;
    }

    return 1;
}

// Finds an entry inside a directory
static int fat32_find_entry(uint32_t cluster, const char *name, int length,
                            struct fat32_dirent *entry, uint32_t *entry_lba, uint32_t *entry_offset) {
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
                    if (entry_lba) *entry_lba = lba + i;
                    if (entry_offset) *entry_offset = j;
                    return 1;
                }
            }
        }

        cluster = fat32_next_cluster(cluster);
    }

    return 0;
}

// Finds a free 32-byte slot in a directory, extending it if full
static int fat32_alloc_entry_slot(uint32_t cluster, uint32_t *out_lba, uint32_t *out_offset) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    while (cluster >= 2 && !fat32_is_end_cluster(cluster)) {
        uint32_t lba = fat32_cluster_lba(cluster);

        for (int i = 0; i < volume.sectors_per_cluster; i++) {
            if (!fat32_read_sector(lba + i, sector))
                return 0;

            for (int j = 0; j < FAT32_SECTOR_SIZE; j += FAT32_DIRECTORY_ENTRY_SIZE) {
                if (sector[j] == 0 || sector[j] == 0xE5) {
                    *out_lba = lba + i;
                    *out_offset = j;
                    return 1;
                }
            }
        }

        uint32_t next = fat32_next_cluster(cluster);

        if (fat32_is_end_cluster(next)) {
            uint8_t empty[FAT32_SECTOR_SIZE];
            uint32_t new_lba;

            next = fat32_alloc_cluster();
            if (next < 2)
                return 0;

            fat32_set_next_cluster(cluster, next);

            for (int i = 0; i < FAT32_SECTOR_SIZE; i++)
                empty[i] = 0;

            new_lba = fat32_cluster_lba(next);
            for (int i = 0; i < volume.sectors_per_cluster; i++)
                fat32_write_sector(new_lba + i, empty);

            *out_lba = new_lba;
            *out_offset = 0;
            return 1;
        }

        cluster = next;
    }

    return 0;
}

// Writes a raw 11-byte-name directory entry
static void fat32_write_entry_raw(uint32_t lba, uint32_t offset, const uint8_t *raw11,
                                   uint8_t attributes, uint32_t first_cluster, uint32_t size) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    if (!fat32_read_sector(lba, sector))
        return;

    for (int i = 0; i < 11; i++)
        sector[offset + i] = raw11[i];

    sector[offset + 11] = attributes;
    for (int i = 12; i < 20; i++)
        sector[offset + i] = 0;

    sector[offset + 20] = (uint8_t)(first_cluster >> 16);
    sector[offset + 21] = (uint8_t)(first_cluster >> 24);
    sector[offset + 26] = (uint8_t)first_cluster;
    sector[offset + 27] = (uint8_t)(first_cluster >> 8);
    sector[offset + 28] = (uint8_t)size;
    sector[offset + 29] = (uint8_t)(size >> 8);
    sector[offset + 30] = (uint8_t)(size >> 16);
    sector[offset + 31] = (uint8_t)(size >> 24);

    fat32_write_sector(lba, sector);
}

// Writes a directory entry for a plain 8.3 name
static int fat32_write_entry(uint32_t lba, uint32_t offset, const char *name, int length,
                              uint8_t attributes, uint32_t first_cluster, uint32_t size) {
    uint8_t raw[11];

    if (!fat32_format_name(name, length, raw))
        return 0;

    fat32_write_entry_raw(lba, offset, raw, attributes, first_cluster, size);
    return 1;
}

// Updates only the size/first_cluster fields of an existing directory entry
static void fat32_update_entry(uint32_t lba, uint32_t offset, uint32_t first_cluster, uint32_t size) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    if (!fat32_read_sector(lba, sector))
        return;

    sector[offset + 20] = (uint8_t)(first_cluster >> 16);
    sector[offset + 21] = (uint8_t)(first_cluster >> 24);
    sector[offset + 26] = (uint8_t)first_cluster;
    sector[offset + 27] = (uint8_t)(first_cluster >> 8);
    sector[offset + 28] = (uint8_t)size;
    sector[offset + 29] = (uint8_t)(size >> 8);
    sector[offset + 30] = (uint8_t)(size >> 16);
    sector[offset + 31] = (uint8_t)(size >> 24);

    fat32_write_sector(lba, sector);
}

// Marks a directory entry as deleted, 0xE5 marker
static void fat32_delete_entry(uint32_t lba, uint32_t offset) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    if (!fat32_read_sector(lba, sector))
        return;

    sector[offset] = 0xE5;
    fat32_write_sector(lba, sector);
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
    uint32_t entry_lba = 0;
    uint32_t entry_offset = 0;
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
        file->entry_lba = 0;
        file->entry_offset = 0;
        return 1;
    }

    while (*part) {
        const char *next = part;
        int length;

        while (*next && *next != '/')
            next++;

        length = (int)(next - part);
        if (length == 0 || !fat32_find_entry(cluster, part, length, &entry, &entry_lba, &entry_offset))
            return 0;

        while (*next == '/')
            next++;

        if (*next == '\0') {
            file->first_cluster = entry.first_cluster;
            file->size = entry.size;
            file->offset = 0;
            file->attributes = entry.attributes;
            file->entry_lba = entry_lba;
            file->entry_offset = entry_offset;
            return 1;
        }

        if (!(entry.attributes & FAT32_ATTRIBUTE_DIRECTORY))
            return 0;

        cluster = entry.first_cluster;
        part = next;
    }

    return 0;
}

// Splits the path into a parent directory cluster and the final component name
// parent_buffer must be at least as long as path
static int fat32_split_path(const char *path, char *parent_buffer, const char **name, int *name_length,
                             uint32_t *parent_cluster) {
    const char *slash = path;
    int parent_length;

    if (!path || path[0] == '\0')
        return 0;

    for (const char *p = path; *p; p++)
        if (*p == '/')
            slash = p + 1;

    *name = slash;
    *name_length = (int)k_strlen(slash);

    if (*name_length == 0 || *name_length > 12)
        return 0;

    parent_length = (int)(slash - path);
    for (int i = 0; i < parent_length; i++)
        parent_buffer[i] = path[i];
    parent_buffer[parent_length] = '\0';

    return fat32_directory_cluster(parent_buffer, parent_cluster);
}

int fat32_create(const char *path) {
    char parent_buffer[128];
    struct fat32_dirent existing;
    const char *name;
    int name_length;
    uint32_t parent_cluster, lba, offset;

    if (!fat32_ready || !path)
        return 0;

    if (!fat32_split_path(path, parent_buffer, &name, &name_length, &parent_cluster))
        return 0;

    if (fat32_find_entry(parent_cluster, name, name_length, &existing, 0, 0))
        return 0;

    if (!fat32_alloc_entry_slot(parent_cluster, &lba, &offset))
        return 0;

    return fat32_write_entry(lba, offset, name, name_length, 0, 0, 0);
}

int fat32_mkdir(const char *path) {
    char parent_buffer[128];
    struct fat32_dirent existing;
    const char *name;
    int name_length;
    uint32_t parent_cluster, lba, offset, new_cluster, new_lba;
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint8_t dot[11]    = {'.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    uint8_t dotdot[11] = {'.', '.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};

    if (!fat32_ready || !path)
        return 0;

    if (!fat32_split_path(path, parent_buffer, &name, &name_length, &parent_cluster))
        return 0;

    if (fat32_find_entry(parent_cluster, name, name_length, &existing, 0, 0))
        return 0;

    new_cluster = fat32_alloc_cluster();
    if (new_cluster < 2)
        return 0;

    for (int i = 0; i < FAT32_SECTOR_SIZE; i++)
        sector[i] = 0;

    new_lba = fat32_cluster_lba(new_cluster);
    for (int i = 0; i < volume.sectors_per_cluster; i++)
        fat32_write_sector(new_lba + i, sector);

    fat32_write_entry_raw(new_lba, 0, dot, FAT32_ATTRIBUTE_DIRECTORY, new_cluster, 0);
    fat32_write_entry_raw(new_lba, 32, dotdot, FAT32_ATTRIBUTE_DIRECTORY, parent_cluster, 0);

    if (!fat32_alloc_entry_slot(parent_cluster, &lba, &offset))
        return 0;

    return fat32_write_entry(lba, offset, name, name_length, FAT32_ATTRIBUTE_DIRECTORY, new_cluster, 0);
}

int fat32_remove(const char *path) {
    struct fat32_file file;
    uint32_t cluster;

    if (!fat32_ready || !fat32_open(path, &file) || file.entry_lba == 0)
        return 0;

    if (file.attributes & FAT32_ATTRIBUTE_DIRECTORY)
        return 0;

    cluster = file.first_cluster;
    while (cluster >= 2 && !fat32_is_end_cluster(cluster)) {
        uint32_t next = fat32_next_cluster(cluster);
        fat32_set_next_cluster(cluster, 0);
        cluster = next;
    }

    fat32_delete_entry(file.entry_lba, file.entry_offset);
    return 1;
}

int fat32_rename(const char *old_path, const char *new_path) {
    struct fat32_file source;
    struct fat32_dirent existing;
    char parent_buffer[128];
    const char *name;
    int name_length;
    uint32_t parent_cluster, lba, offset;

    if (!fat32_ready || !fat32_open(old_path, &source) || source.entry_lba == 0)
        return 0;

    if (!fat32_split_path(new_path, parent_buffer, &name, &name_length, &parent_cluster))
        return 0;

    if (fat32_find_entry(parent_cluster, name, name_length, &existing, 0, 0))
        return 0;

    if (!fat32_alloc_entry_slot(parent_cluster, &lba, &offset))
        return 0;

    if (!fat32_write_entry(lba, offset, name, name_length, source.attributes, source.first_cluster, source.size))
        return 0;

    fat32_delete_entry(source.entry_lba, source.entry_offset);
    return 1;
}

// Checks whether a directory cluster chain has no entries besides "." and ".."
static int fat32_directory_empty(uint32_t cluster) {
    uint8_t sector[FAT32_SECTOR_SIZE];

    while (cluster >= 2 && !fat32_is_end_cluster(cluster)) {
        uint32_t lba = fat32_cluster_lba(cluster);

        for (int i = 0; i < volume.sectors_per_cluster; i++) {
            if (!fat32_read_sector(lba + i, sector))
                return 0;

            for (int j = 0; j < FAT32_SECTOR_SIZE; j += FAT32_DIRECTORY_ENTRY_SIZE) {
                if (sector[j] == 0)
                    return 1;

                if (fat32_entry_visible(sector + j))
                    return 0;
            }
        }

        cluster = fat32_next_cluster(cluster);
    }

    return 1;
}

int fat32_rmdir(const char *path) {
    struct fat32_file dir;
    uint32_t cluster;

    if (!fat32_ready || !fat32_open(path, &dir) || dir.entry_lba == 0)
        return 0;

    if (!(dir.attributes & FAT32_ATTRIBUTE_DIRECTORY))
        return 0;

    if (!fat32_directory_empty(dir.first_cluster))
        return 0;

    cluster = dir.first_cluster;
    while (cluster >= 2 && !fat32_is_end_cluster(cluster)) {
        uint32_t next = fat32_next_cluster(cluster);
        fat32_set_next_cluster(cluster, 0);
        cluster = next;
    }

    fat32_delete_entry(dir.entry_lba, dir.entry_offset);
    return 1;
}

uint32_t fat32_write(struct fat32_file *file, const void *buffer, uint32_t size) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    const uint8_t *input = (const uint8_t *)buffer;
    uint32_t written = 0;
    uint32_t cluster_size;

    if (!fat32_ready || !file || !buffer || (file->attributes & FAT32_ATTRIBUTE_DIRECTORY))
        return 0;

    cluster_size = volume.sectors_per_cluster * FAT32_SECTOR_SIZE;

    if (file->first_cluster < 2) {
        file->first_cluster = fat32_alloc_cluster();
        if (file->first_cluster < 2)
            return 0;
    }

    while (written < size) {
        uint32_t cluster = file->first_cluster;
        uint32_t cluster_index = file->offset / cluster_size;
        uint32_t cluster_offset = file->offset % cluster_size;
        uint32_t sector_index = cluster_offset / FAT32_SECTOR_SIZE;
        uint32_t byte_offset = cluster_offset % FAT32_SECTOR_SIZE;
        uint32_t chunk = FAT32_SECTOR_SIZE - byte_offset;

        // iterate/extend the chain until the cluster that holds this offset
        while (cluster_index-- > 0) {
            uint32_t next = fat32_next_cluster(cluster);

            if (fat32_is_end_cluster(next)) {
                next = fat32_alloc_cluster();
                if (next < 2)
                    return written;
                fat32_set_next_cluster(cluster, next);
            }

            cluster = next;
        }

        if (chunk > size - written)
            chunk = size - written;

        // preserve the bytes we are not overwriting
        if ((byte_offset != 0 || chunk < FAT32_SECTOR_SIZE) &&
            !fat32_read_sector(fat32_cluster_lba(cluster) + sector_index, sector))
            break;

        for (uint32_t i = 0; i < chunk; i++)
            sector[byte_offset + i] = input[written + i];

        if (!fat32_write_sector(fat32_cluster_lba(cluster) + sector_index, sector))
            break;

        written += chunk;
        file->offset += chunk;

        if (file->offset > file->size)
            file->size = file->offset;
    }

    if (written > 0 && file->entry_lba != 0)
        fat32_update_entry(file->entry_lba, file->entry_offset, file->first_cluster, file->size);

    return written;
}

uint32_t fat32_read(struct fat32_file *file, void *buffer, uint32_t size) {
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint8_t *output = (uint8_t *)buffer;
    uint32_t read = 0;
    uint32_t cluster_size;

    if (!fat32_ready || !file || !buffer || (file->attributes & FAT32_ATTRIBUTE_DIRECTORY))
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