#include "../include/sysinfo.h"
#include "../include/kstring.h"
#include "../include/memory.h"
#include "../include/io.h"
#include "../include/ata.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71
// PCI config space //
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static void system_read_rtc(struct system_info *info);
static void system_read_cpu(struct system_info *info);
static void system_read_gpu(struct system_info *info);
static uint8_t cmos_read(uint8_t reg);
static int cmos_updating(void);
static int bcd_to_binary(uint8_t value);


void system_getinfo(struct system_info *info) {
    struct memory_stats mem_stats;
    memory_get_stats(&mem_stats);

    k_strcat(info->os_name, OS_NAME, OS_MODEL, OS_SEP);
    k_strcp(info->architecture, ARCH_STRING);
    k_strcat(info->build, __DATE__, __TIME__, " @ ");
    info->disk_mb = ata_is_ready() ? ata_get_info()->size_mb : 0;
    info->ram_kib = mem_stats.total_bytes / 1024;
    info->heap_used = mem_stats.used_bytes;
    info->heap_free = mem_stats.free_bytes;
    system_read_rtc(info);                  // Date + Time "info" fields
    system_read_cpu(info);
    system_read_gpu(info);
}

// CPUID helpers
static void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf)
        :
    );
}

// Copy 4 bytes from a uint32 into buf (little-endian, as CPUID delivers) 
static void u32_to_str(char *buf, uint32_t val) {
    buf[0] = (char)(val & 0xFF);
    buf[1] = (char)((val >> 8) & 0xFF);
    buf[2] = (char)((val >> 16) & 0xFF);
    buf[3] = (char)((val >> 24) & 0xFF);
}

static void system_read_cpu(struct system_info *info) {
    uint32_t eax, ebx, ecx, edx;

    // Check whether extended brand string is supported 
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000004) {
        // Fallback 
        // read vendor string (12 chars) from leaf 0 
        cpuid(0, &eax, &ebx, &ecx, &edx);
        u32_to_str(info->cpu,      ebx);
        u32_to_str(info->cpu + 4,  edx);
        u32_to_str(info->cpu + 8,  ecx);
        info->cpu[12] = '\0';
        return;
    }

    /*
     * Brand string - three consecutive leaves 0x80000002–0x80000004,
     * each returning 16 bytes (4 regs × 4 bytes). Total = 48 bytes + NUL.
     */
    char *p = info->cpu;
    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        cpuid(leaf, &eax, &ebx, &ecx, &edx);
        u32_to_str(p,      eax);
        u32_to_str(p + 4,  ebx);
        u32_to_str(p + 8,  ecx);
        u32_to_str(p + 12, edx);
        p += 16;
    }
    info->cpu[48] = '\0';

    // Trim leading spaces (brand strings often start with spaces) 
    char *start = info->cpu;
    while (*start == ' ') start++;
    if (start != info->cpu) {
        char *dst = info->cpu;
        while (*start) *dst++ = *start++;
        *dst = '\0';
    }
}

// PCI helpers
static uint32_t pci_read32(uint8_t bus, uint8_t slot,
                            uint8_t func, uint8_t offset)
{
    uint32_t addr = (uint32_t)(1u << 31)
                  | ((uint32_t)bus  << 16)
                  | ((uint32_t)slot << 11)
                  | ((uint32_t)func << 8)
                  | ((uint32_t)(offset & 0xFC));
    outl(PCI_CONFIG_ADDRESS, addr);
    return inl(PCI_CONFIG_DATA);
}

// Map well-known GPU vendor/device ID pairs to a human string 
static const char *gpu_identify(uint16_t vendor, uint16_t device) {
    // NVIDIA 
    if (vendor == 0x10DE) return "NVIDIA GPU";

    // AMD / ATI 
    if (vendor == 0x1002) return "AMD/ATI GPU";

    // Intel integrated 
    if (vendor == 0x8086) {
        // Display class = 0x03, subclass = 0x00/0x02/0x80
        (void)device;
        return "Intel Integrated Graphics";
    }

    // VMware SVGA
    if (vendor == 0x15AD && device == 0x0405) return "VMware SVGA II";
    if (vendor == 0x15AD && device == 0x0406) return "VMware SVGA III";

    // VirtualBox VGA //
    if (vendor == 0x80EE && device == 0xBEEF) return "VirtualBox VGA";

    // QEMU stdvga / Bochs VBE //
    if (vendor == 0x1234 && device == 0x1111) return "QEMU/Bochs VGA";

    return NULL;
}

static void system_read_gpu(struct system_info *info) {
    /*
     * Scan PCI bus 0, slots 0–31, function 0 looking for a display
     * controller (class 0x03). Stops at the first match.
     */
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t id  = pci_read32(0, slot, 0, 0x00);            // vendor + device 
        if (id == 0xFFFFFFFF) continue;                         // empty slot      

        uint32_t cls = pci_read32(0, slot, 0, 0x08);            // class/subclass  
        uint8_t  base_class = (uint8_t)((cls >> 24) & 0xFF);

        if (base_class != 0x03) continue;                       // not display     

        uint16_t vendor = (uint16_t)(id & 0xFFFF);
        uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);

        const char *name = gpu_identify(vendor, device);
        if (name) {
            k_strcp(info->gpu, name);
        } else {
            // Build "VEN:XXXX DEV:XXXX" 
            char tmp[5];
            k_strcp(info->gpu, "VEN:");
            // manual hex render into tmp 
            const char hex[] = "0123456789ABCDEF";
            tmp[0] = hex[(vendor >> 12) & 0xF];
            tmp[1] = hex[(vendor >>  8) & 0xF];
            tmp[2] = hex[(vendor >>  4) & 0xF];
            tmp[3] = hex[ vendor        & 0xF];
            tmp[4] = '\0';
            k_strapp(info->gpu, tmp);
            k_strapp(info->gpu, " DEV:");
            tmp[0] = hex[(device >> 12) & 0xF];
            tmp[1] = hex[(device >>  8) & 0xF];
            tmp[2] = hex[(device >>  4) & 0xF];
            tmp[3] = hex[ device        & 0xF];
            tmp[4] = '\0';
            k_strapp(info->gpu, tmp);
        }
        return;
    }

    k_strcp(info->gpu, "Unknown");
}

static uint8_t cmos_read(uint8_t reg) {
  outb(CMOS_ADDRESS, reg);
  return inb(CMOS_DATA);
}

static int cmos_updating(void) {
  return (cmos_read(0x0A) & 0x80) != 0;
}

static int bcd_to_binary(uint8_t value) {
  return (value & 0x0F) + ((value / 16) * 10);
}

static void system_read_rtc(struct system_info *info) {
  uint8_t second, minute, hour, day, month, year, status_b;
  uint8_t last_second, last_minute, last_hour, last_day, last_month, last_year;
  int pm;

  while (cmos_updating()) {}

  second = cmos_read(0x00);
  minute = cmos_read(0x02);
  hour   = cmos_read(0x04);
  day    = cmos_read(0x07);
  month  = cmos_read(0x08);
  year   = cmos_read(0x09);
  status_b = cmos_read(0x0B);

  // Read again and compare if any field changed
  do {
    last_second = second;
    last_minute = minute;
    last_hour   = hour;
    last_day    = day;
    last_month  = month;
    last_year   = year;

    while (cmos_updating()) {}

    second = cmos_read(0x00);
    minute = cmos_read(0x02);
    hour   = cmos_read(0x04);
    day    = cmos_read(0x07);
    month  = cmos_read(0x08);
    year   = cmos_read(0x09);
    status_b = cmos_read(0x0B);
  } while (second != last_second || minute != last_minute || hour != last_hour ||
           day != last_day || month != last_month || year != last_year);

  // higher bit = PM (if in 12h mode)
  pm = (hour & 0x80) != 0;
  hour &= 0x7F;

  // Only convert from BCD if bit 2 of register B is 0
  if ((status_b & 0x04) == 0) {
    second = (uint8_t)bcd_to_binary(second);
    minute = (uint8_t)bcd_to_binary(minute);
    hour   = (uint8_t)bcd_to_binary(hour);
    day    = (uint8_t)bcd_to_binary(day);
    month  = (uint8_t)bcd_to_binary(month);
    year   = (uint8_t)bcd_to_binary(year);
  }

  // If in 12h mode (bit 1 of register B = 0), converts into 24h
  if ((status_b & 0x02) == 0) {
    if (pm && hour < 12) {
      hour += 12;
    } else if (!pm && hour == 12) {
      hour = 0;
    }
  }

  info->second = second;
  info->minute = minute;
  info->hour   = hour;
  info->day    = day;
  info->month  = month;
  info->year   = year;
}