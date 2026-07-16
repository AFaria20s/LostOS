#include <stdint.h>

#include "../include/vfs.h"
#include "../include/fat32.h"
#include "../include/kstring.h"

static int vfs_ready = 0;

int vfs_init(void) {
    vfs_ready = fat32_init();
    return vfs_ready;
}

int vfs_open(const char *path, struct vfs_file *file) {
    return fat32_open(path, &file->fat32);
}

uint32_t vfs_read(struct vfs_file *file, void *buf, uint32_t size) {
    return fat32_read(&file->fat32, buf, size);
}

uint32_t vfs_write(struct vfs_file *file, const void *buf, uint32_t size) {
    return fat32_write(&file->fat32, buf, size);
}

int vfs_create(const char *path) {
    return fat32_create(path);
}

int vfs_mkdir(const char *path) {
    return fat32_mkdir(path);
}

int vfs_remove(const char *path) {
    return fat32_remove(path);
}

int vfs_rename(const char *old_path, const char *new_path) {
    return fat32_rename(old_path, new_path);
}

int vfs_rmdir(const char *path) {
    return fat32_rmdir(path);
}

int vfs_readdir(const char *path, int index, struct vfs_dirent *entry) {
    struct fat32_dirent fat_entry;
    if (!fat32_readdir(path, index, &fat_entry)) return 0;

    entry->size = fat_entry.size;
    entry->attributes = fat_entry.attributes;
    k_strcp(entry->name, fat_entry.name);
    
    return 1;
}

int vfs_is_ready(void) {
    return vfs_ready;
}