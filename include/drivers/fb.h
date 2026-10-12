#ifndef FB_H
#define FB_H

#include <stdint.h>

#include "mm/memory.h"

struct framebuffer {
  uintptr_t address;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  uint8_t bpp;
};

int fb_init(multiboot_info_t *mbi);
uintptr_t fb_get_address(void);
uint32_t fb_get_pitch(void);
uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint8_t fb_get_bpp(void);

#endif
