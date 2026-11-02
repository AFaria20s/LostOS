#include <stdint.h>

#include "fs/vfs.h"
#include "fs/fat32.h"
#include "lib/kstring.h"

static int vfs_ready = 0;

int vfs_init(void) {
    vfs_ready = fat32_init();
    return vfs_ready;
}

enum vfs_result vfs_open(const char *path, struct vfs_file *file) {
    int result;

    if (!path || !file)
        return VFS_INVALID_PATH;

    result = fat32_open(path, &file->fat32);
    switch (result) {
    case FAT32_OPEN_OK:
        return VFS_OK;
    case FAT32_OPEN_NOT_FOUND:
        return VFS_NOT_FOUND;
    case FAT32_OPEN_INVALID_PATH:
        return VFS_INVALID_PATH;
    case FAT32_OPEN_IO_ERROR:
    default:
        return VFS_IO_ERROR;
    }
}

uint32_t vfs_read(struct vfs_file *file, void *buf, uint32_t size) {
    return fat32_read(&file->fat32, buf, size);
}

uint32_t vfs_write(struct vfs_file *file, const void *buf, uint32_t size) {
    return fat32_write(&file->fat32, buf, size);
}

enum vfs_result vfs_create(const char *path) {
    int result;

    if (!path)
        return VFS_INVALID_PATH;

    result = fat32_create(path);
    switch (result) {
    case FAT32_CREATE_OK:
        return VFS_OK;
    case FAT32_CREATE_EXISTS:
        return VFS_EXISTS;
    case FAT32_CREATE_INVALID_PATH:
        return VFS_INVALID_PATH;
    case FAT32_CREATE_NOT_FOUND:
        return VFS_NOT_FOUND;
    case FAT32_CREATE_IO_ERROR:
    default:
        return VFS_IO_ERROR;
    }
}

enum vfs_result vfs_mkdir(const char *path) {
    int result;

    if (!path)
        return VFS_INVALID_PATH;

    result = fat32_mkdir(path);
    switch (result) {
    case FAT32_MKDIR_OK:
        return VFS_OK;
    case FAT32_MKDIR_EXISTS:
        return VFS_EXISTS;
    case FAT32_MKDIR_INVALID_PATH:
        return VFS_INVALID_PATH;
    case FAT32_MKDIR_NOT_FOUND:
        return VFS_NOT_FOUND;
    case FAT32_MKDIR_IO_ERROR:
    default:
        return VFS_IO_ERROR;
    }
}

enum vfs_result vfs_remove(const char *path) {
    int result;

    if (!path)
        return VFS_INVALID_PATH;

    result = fat32_remove(path);
    switch (result) {
    case FAT32_REMOVE_OK:
        return VFS_OK;
    case FAT32_REMOVE_NOT_FOUND:
        return VFS_NOT_FOUND;
    case FAT32_REMOVE_INVALID_PATH:
        return VFS_INVALID_PATH;
    case FAT32_REMOVE_IS_DIRECTORY:
        return VFS_IS_DIRECTORY;
    case FAT32_REMOVE_IO_ERROR:
    default:
        return VFS_IO_ERROR;
    }
}

int vfs_rename(const char *old_path, const char *new_path) {
    return fat32_rename(old_path, new_path);
}

int vfs_rmdir(const char *path) {
    return fat32_rmdir(path);
}

int vfs_ensure_parents(const char *path) {
    char partial[256];
    int i = 0;

    while (path[i]) {
        if (path[i] == '/' && i > 0) {
            k_strcp(partial, sizeof(partial), path);
            partial[i] = '\0';

            if (!vfs_is_directory(partial) &&
                vfs_mkdir(partial) != VFS_OK)
                return 0;
        }
        i++;
    }
    return 1;
}

int vfs_readdir(const char *path, int index, struct vfs_dirent *entry) {
    struct fat32_dirent fat_entry;
    if (!fat32_readdir(path, index, &fat_entry)) return 0;

    entry->size = fat_entry.size;
    entry->attributes = fat_entry.attributes;
    k_strcp(entry->name, sizeof(entry->name), fat_entry.name);
    
    return 1;
}

int vfs_is_ready(void) {
    return vfs_ready;
}

int vfs_is_directory(const char *path) {
    struct vfs_file file;
    if (vfs_open(path, &file) != VFS_OK) return 0;
    return file.fat32.attributes & 0x10;
}