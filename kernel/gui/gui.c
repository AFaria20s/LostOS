#include <stdint.h>

#include "drivers/fb.h"
#include "drivers/mouse.h"
#include "gui/gui.h"
#include "gui/icons.h"

#define GUI_BACKGROUND 0x0009080A
#define GUI_SHADOW      0x00000000
#define GUI_LIGHT       0x00FFE7A6
#define GUI_ACCENT      0x00D99A2B
#define GUI_CURSOR_SCALE 1

static uint32_t previous_cursor_x;
static uint32_t previous_cursor_y;
static int cursor_drawn;

static void gui_fill_clipped(uint32_t x, uint32_t y, uint32_t width,
                             uint32_t height, uint32_t color,
                             uint32_t clip_x, uint32_t clip_y,
                             uint32_t clip_width, uint32_t clip_height) {
  uint32_t left = x > clip_x ? x : clip_x;
  uint32_t top = y > clip_y ? y : clip_y;
  uint64_t right = (uint64_t)x + width;
  uint64_t bottom = (uint64_t)y + height;
  uint64_t clip_right = (uint64_t)clip_x + clip_width;
  uint64_t clip_bottom = (uint64_t)clip_y + clip_height;

  if (right > clip_right)
    right = clip_right;
  if (bottom > clip_bottom)
    bottom = clip_bottom;
  if (right > left && bottom > top)
    fb_fill_rect(left, top, (uint32_t)right - left, (uint32_t)bottom - top,
                 color);
}

static void gui_draw_desktop_region(uint32_t x, uint32_t y, uint32_t width,
                                    uint32_t height) {
  uint32_t screen_width = fb_get_width();
  uint32_t screen_height = fb_get_height();

  gui_fill_clipped(0, 0, screen_width, screen_height, GUI_BACKGROUND,
                   x, y, width, height);
}

static void gui_clamp_cursor(uint32_t *x, uint32_t *y) {
  uint32_t width = fb_get_width();
  uint32_t height = fb_get_height();
  uint32_t cursor_width = gui_icon_cursor.width * GUI_CURSOR_SCALE;
  uint32_t cursor_height = gui_icon_cursor.height * GUI_CURSOR_SCALE;

  if (width > cursor_width && *x > width - cursor_width)
    *x = width - cursor_width;
  if (height > cursor_height && *y > height - cursor_height)
    *y = height - cursor_height;
}

void gui_init(void) {
  mouse_set_bounds(fb_get_width(), fb_get_height());
  cursor_drawn = 0;
}

void gui_render(void) {
  uint32_t cursor_x = mouse_get_x();
  uint32_t cursor_y = mouse_get_y();

  gui_clamp_cursor(&cursor_x, &cursor_y);
  if (cursor_drawn)
    gui_draw_desktop_region(previous_cursor_x, previous_cursor_y,
                            gui_icon_cursor.width * GUI_CURSOR_SCALE,
                            gui_icon_cursor.height * GUI_CURSOR_SCALE);
  else
    gui_draw_desktop_region(0, 0, fb_get_width(), fb_get_height());

  gui_draw_icon(&gui_icon_cursor, cursor_x, cursor_y, GUI_CURSOR_SCALE,
                GUI_SHADOW, GUI_LIGHT, GUI_ACCENT);
  previous_cursor_x = cursor_x;
  previous_cursor_y = cursor_y;
  cursor_drawn = 1;
}
