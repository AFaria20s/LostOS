#ifndef SYSINFO_H
#define SYSINFO_H

#include <stdint.h>
#include <stddef.h>

#define OS_NAME  "0x194"
#define OS_MODEL "1.0_SLOTH"
#define OS_SEP   " - "

#if defined(__x86_64__)
  #define ARCH_STRING "x86_64 (long mode, 64-bit)"
#elif defined(__i686__)
  #define ARCH_STRING "i686 (protected mode, 32-bit)"
#else
  #define ARCH_STRING "x86 (unknown)"
#endif

struct system_info {
    char os_name[sizeof(OS_NAME) + sizeof(OS_SEP) + sizeof(OS_MODEL) - 2];
    char architecture[sizeof(ARCH_STRING)];
    char build[sizeof(__DATE__) + sizeof(" @ ") + sizeof(__TIME__) - 2];
    char cpu[49];
    char gpu[32];
    size_t ram_kib;
    size_t heap_used;
    size_t heap_free;
    uint32_t disk_mib;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    uint8_t year;   
};

void system_getinfo(struct system_info *info);

#endif