#include <stdint.h>
#include "fs/fat32.h"

struct vfs_file {
    int fs_type;
    struct fat32_file fat32;
};

struct vfs_dirent {
    char name[13];
    uint32_t size;
    uint8_t attributes;
};

enum vfs_result {
    VFS_OK,
    VFS_EOF,
    VFS_NOT_FOUND,
    VFS_EXISTS,
    VFS_NOT_EMPTY,
    VFS_IS_DIRECTORY,
    VFS_INVALID_PATH,
    VFS_IO_ERROR
};

int vfs_init(void);
int vfs_is_ready(void);

enum vfs_result vfs_open(const char *path, struct vfs_file *file);
enum vfs_result vfs_readdir(const char *path, int index, struct vfs_dirent *entry);
enum vfs_result vfs_read_status(struct vfs_file *file, void *buf, uint32_t size,
                                uint32_t *read);
enum vfs_result vfs_write_status(struct vfs_file *file, const void *buf,
                                 uint32_t size, uint32_t *written);

uint32_t vfs_read(struct vfs_file *file, void *buf, uint32_t size);
uint32_t vfs_write(struct vfs_file *file, const void *buf, uint32_t size);

enum vfs_result vfs_create(const char *path);
enum vfs_result vfs_mkdir(const char *path);
enum vfs_result vfs_remove(const char *path);
enum vfs_result vfs_rename(const char *old_path, const char *new_path);
enum vfs_result vfs_rmdir(const char *path);

int vfs_ensure_parents(const char *path);
int vfs_is_directory(const char *path);