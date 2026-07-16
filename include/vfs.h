#include <stdint.h>
#include "../include/fat32.h"

struct vfs_file {
    int fs_type;
    struct fat32_file fat32;
};

struct vfs_dirent {
    char name[13];
    uint32_t size;
    uint8_t attributes;
};

int vfs_init(void);
int vfs_is_ready(void);

int vfs_open(const char *path, struct vfs_file *file);
int vfs_readdir(const char *path, int index, struct vfs_dirent *entry);

uint32_t vfs_read(struct vfs_file *file, void *buf, uint32_t size);
uint32_t vfs_write(struct vfs_file *file, const void *buf, uint32_t size);

int vfs_create(const char *path);
int vfs_mkdir(const char *path);
int vfs_remove(const char *path);
int vfs_rename(const char *old_path, const char *new_path);
int vfs_rmdir(const char *path);