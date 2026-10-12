#include <stdint.h>

#include "drivers/fb.h"

static struct framebuffer framebuffer;
static int framebuffer_ready;
static uintptr_t draw_address;
static uint32_t draw_pitch;
static int backbuffer_enabled;

// The configured GRUB mode uses 32-bit 0x00RRGGBB pixels.

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
  draw_address = framebuffer.address;
  draw_pitch = framebuffer.pitch;
  backbuffer_enabled = 0;
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

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
  volatile uint32_t *pixel;

  if (!framebuffer_ready || x >= framebuffer.width || y >= framebuffer.height)
    return;

  pixel = (volatile uint32_t *)(draw_address +
                                y * draw_pitch +
                                x * (framebuffer.bpp / 8));
  *pixel = color;
}

void fb_clear(uint32_t color) {
  fb_fill_rect(0, 0, framebuffer.width, framebuffer.height, color);
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                  uint32_t color) {
  uint64_t right;
  uint64_t bottom;
  uint32_t clipped_width;
  uint32_t clipped_height;

  if (!framebuffer_ready || x >= framebuffer.width || y >= framebuffer.height)
    return;

  right = (uint64_t)x + width;
  bottom = (uint64_t)y + height;
  if (right > framebuffer.width)
    right = framebuffer.width;
  if (bottom > framebuffer.height)
    bottom = framebuffer.height;

  clipped_width = (uint32_t)right - x;
  clipped_height = (uint32_t)bottom - y;

  for (uint32_t row = 0; row < clipped_height; row++)
    for (uint32_t column = 0; column < clipped_width; column++)
      fb_put_pixel(x + column, y + row, color);
}

int fb_set_backbuffer(void *buffer, uint32_t pitch) {
  if (!framebuffer_ready)
    return -1;

  if (!buffer) {
    draw_address = framebuffer.address;
    draw_pitch = framebuffer.pitch;
    backbuffer_enabled = 0;
    return 0;
  }
  if (pitch < framebuffer.pitch)
    return -1;

  draw_address = (uintptr_t)buffer;
  draw_pitch = pitch;
  backbuffer_enabled = 1;
  return 0;
}

int fb_uses_backbuffer(void) {
  return backbuffer_enabled;
}

void fb_present_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
  uint32_t right;
  uint32_t bottom;

  if (!framebuffer_ready || !backbuffer_enabled || x >= framebuffer.width ||
      y >= framebuffer.height)
    return;

  right = width > framebuffer.width - x ? framebuffer.width : x + width;
  bottom = height > framebuffer.height - y ? framebuffer.height : y + height;
  for (uint32_t row = y; row < bottom; row++) {
    const uint32_t *source = (const uint32_t *)(draw_address + row * draw_pitch +
                                                x * sizeof(uint32_t));
    volatile uint32_t *target = (volatile uint32_t *)(framebuffer.address +
                                                       row * framebuffer.pitch +
                                                       x * sizeof(uint32_t));
    for (uint32_t column = x; column < right; column++)
      *target++ = *source++;
  }
}

void fb_present(void) {
  fb_present_rect(0, 0, framebuffer.width, framebuffer.height);
}
