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
    uint32_t entry_lba;    // sector holding this file's directory entry (0 = none, e.g. root)
    uint32_t entry_offset; // byte offset of the entry within that sector
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

// Writes bytes to an open file, allocating clusters as needed
// Returns the number of bytes written
uint32_t fat32_write(struct fat32_file *file, const void *buffer, uint32_t size);

// Creates an empty file at path
// Returns 1 on success, 0 if it already exists or the parent doesn't
int fat32_create(const char *path);

// Creates a directory at path
// Returns 1 on success, 0 if it already exists or the parent doesn't
int fat32_mkdir(const char *path);

// Reads an entry from a directory
// Returns 1 when the entry exists
int fat32_readdir(const char *path, int index, struct fat32_dirent *entry);

#endif