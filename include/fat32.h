#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#define FAT32_NAME_MAX 12

// Stores an open FAT32 file
struct fat32_file {
    uint32_t first_cluster;
    uint32_t size;
    uint32_t offset;
    uint8_t attributes;
};

// Stores a FAT32 directory entry
struct fat32_dirent {
    char name[FAT32_NAME_MAX + 1];
    uint32_t first_cluster;
    uint32_t size;
    uint8_t attributes;
};

// Mounts the FAT32 partition
// Returns 1 on success
int fat32_init(void);

// Opens a file or directory by path
// Returns 1 on success
int fat32_open(const char *path, struct fat32_file *file);

// Reads bytes from an open file
// Returns the number of bytes read
uint32_t fat32_read(struct fat32_file *file, void *buffer, uint32_t size);

// Reads an entry from a directory
// Returns 1 when the entry exists
int fat32_readdir(const char *path, int index, struct fat32_dirent *entry);

#endif
