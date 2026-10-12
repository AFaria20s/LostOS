#include <stdint.h>

#include "drivers/fb.h"

static struct framebuffer framebuffer;
static int framebuffer_ready;

int fb_init(multiboot_info_t *mbi) {
  uint32_t address;

  framebuffer_ready = 0;

  if (!mbi || !(mbi->flags & (1 << 12)))
    return -1;

  address = (uint32_t)mbi->framebuffer_addr;
  if (mbi->framebuffer_type != 1 || mbi->framebuffer_bpp != 32 ||
      address == 0)
    return -1;

  framebuffer.address = (uintptr_t)address;
  framebuffer.pitch = mbi->framebuffer_pitch;
  framebuffer.width = mbi->framebuffer_width;
  framebuffer.height = mbi->framebuffer_height;
  framebuffer.bpp = mbi->framebuffer_bpp;
  framebuffer_ready = 1;
  return 0;
}

uintptr_t fb_get_address(void) {
  return framebuffer_ready ? framebuffer.address : 0;
}

uint32_t fb_get_pitch(void) {
  return framebuffer_ready ? framebuffer.pitch : 0;
}

uint32_t fb_get_width(void) {
  return framebuffer_ready ? framebuffer.width : 0;
}

uint32_t fb_get_height(void) {
  return framebuffer_ready ? framebuffer.height : 0;
}

uint8_t fb_get_bpp(void) {
  return framebuffer_ready ? framebuffer.bpp : 0;
}
