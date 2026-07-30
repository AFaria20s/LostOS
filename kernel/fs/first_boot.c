#include "fs/first_boot.h"
#include "fs/vfs.h"

int first_boot_setup() {
    if(vfs_is_directory("/etc")) 
        return 0;           // already exists, no need to create directories

    vfs_mkdir("/etc");      // configuration files 
    vfs_mkdir("/home");     // home directory
    vfs_mkdir("/bin");      // scripts
    vfs_mkdir("/docs");     // general OS

    return 1;
}