#include <stdint.h>

#include "drivers/fb.h"
#include "drivers/mouse.h"
#include "drivers/vga.h"
#include "gui/font.h"
#include "gui/gui.h"
#include "gui/icons.h"
#include "gui/theme.h"
#include "mm/memory.h"

#define GUI_CURSOR_SCALE 1
#define GUI_TERMINAL_PADDING 16
#define GUI_TERMINAL_TITLE_HEIGHT 24

static uint32_t previous_cursor_x;
static uint32_t previous_cursor_y;
static int cursor_drawn;

struct gui_terminal_layout {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint32_t text_x;
  uint32_t text_y;
};

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

  gui_fill_clipped(0, 0, screen_width, screen_height,
                   gui_theme_default.background,
                   x, y, width, height);
}

static struct gui_terminal_layout gui_terminal_layout(void) {
  struct gui_terminal_layout layout;
  uint32_t text_width = VGA_WIDTH * (GUI_FONT_WIDTH + 1);
  uint32_t text_height = VGA_HEIGHT * (GUI_FONT_HEIGHT + 1);
  uint32_t screen_width = fb_get_width();

  layout.width = text_width + GUI_TERMINAL_PADDING * 2;
  layout.height = text_height + GUI_TERMINAL_PADDING * 2 + GUI_TERMINAL_TITLE_HEIGHT;
  layout.x = screen_width > layout.width ? (screen_width - layout.width) / 2 : 0;
  layout.y = 56;
  layout.text_x = layout.x + GUI_TERMINAL_PADDING;
  layout.text_y = layout.y + GUI_TERMINAL_TITLE_HEIGHT + GUI_TERMINAL_PADDING;
  return layout;
}

static uint32_t gui_terminal_color(uint8_t vga_color) {
  switch (vga_color & 0x0F) {
    case VGA_COLOR_BLACK: return gui_theme_default.background;
    case VGA_COLOR_BROWN:
    case VGA_COLOR_LIGHT_BROWN: return gui_theme_default.accent;
    case VGA_COLOR_DARK_GREY: return gui_theme_default.text_muted;
    case VGA_COLOR_WHITE:
    case VGA_COLOR_LIGHT_GREY: return gui_theme_default.text;
    case VGA_COLOR_LIGHT_CYAN: return gui_theme_default.highlight;
    default: return gui_theme_default.text;
  }
}

static void gui_draw_terminal_region(uint32_t clip_x, uint32_t clip_y,
                                     uint32_t clip_width,
                                     uint32_t clip_height) {
  struct gui_terminal_layout layout = gui_terminal_layout();

  gui_fill_clipped(layout.x, layout.y, layout.width, layout.height,
                   gui_theme_default.border, clip_x, clip_y, clip_width,
                   clip_height);
  gui_fill_clipped(layout.x + 1, layout.y + 1, layout.width - 2,
                   layout.height - 2, gui_theme_default.surface, clip_x,
                   clip_y, clip_width, clip_height);
  gui_fill_clipped(layout.x + 1, layout.y + 1, layout.width - 2,
                   GUI_TERMINAL_TITLE_HEIGHT, gui_theme_default.background,
                   clip_x, clip_y, clip_width, clip_height);

  for (uint32_t row = 0; row < VGA_HEIGHT; row++) {
    for (uint32_t column = 0; column < VGA_WIDTH; column++) {
      uint16_t cell = t_get_cell(column, row);
      char character = (char)(cell & 0xFF);
      uint8_t color = (uint8_t)(cell >> 8);
      uint32_t character_x = layout.text_x + column * (GUI_FONT_WIDTH + 1);
      uint32_t character_y = layout.text_y + row * (GUI_FONT_HEIGHT + 1);

      if (character_x + GUI_FONT_WIDTH > clip_x &&
          character_x < clip_x + clip_width &&
          character_y + GUI_FONT_HEIGHT > clip_y &&
          character_y < clip_y + clip_height)
        gui_draw_char(character_x, character_y, character, 1,
                      gui_terminal_color(color));
    }
  }

  gui_draw_text(layout.x + GUI_TERMINAL_PADDING, layout.y + 8,
                "LOSTOS TERMINAL", 1, gui_theme_default.highlight);
  if (t_column < VGA_WIDTH && t_row < VGA_HEIGHT)
    fb_fill_rect(layout.text_x + t_column * (GUI_FONT_WIDTH + 1),
                 layout.text_y + t_row * (GUI_FONT_HEIGHT + 1) +
                 GUI_FONT_HEIGHT, GUI_FONT_WIDTH, 1, gui_theme_default.accent);
}

static void gui_draw_terminal(void) {
  gui_draw_terminal_region(0, 0, fb_get_width(), fb_get_height());
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
  uint64_t bytes = (uint64_t)fb_get_pitch() * fb_get_height();
  void *backbuffer = 0;

  mouse_set_bounds(fb_get_width(), fb_get_height());
  cursor_drawn = 0;
  if (bytes <= 0xFFFFFFFFU) {
    backbuffer = kmalloc((size_t)bytes);
    if (backbuffer && fb_set_backbuffer(backbuffer, fb_get_pitch()) != 0)
      backbuffer = 0;
  }
}

static void gui_draw_cursor(uint32_t cursor_x, uint32_t cursor_y) {
  gui_draw_icon(&gui_icon_cursor, cursor_x, cursor_y, GUI_CURSOR_SCALE,
                gui_theme_default.shadow, gui_theme_default.highlight,
                gui_theme_default.accent);
}

void gui_render_full(void) {
  uint32_t cursor_x = mouse_get_x();
  uint32_t cursor_y = mouse_get_y();

  gui_clamp_cursor(&cursor_x, &cursor_y);
  gui_draw_desktop_region(0, 0, fb_get_width(), fb_get_height());
  gui_draw_terminal();
  gui_draw_cursor(cursor_x, cursor_y);
  if (fb_uses_backbuffer())
    fb_present();
  previous_cursor_x = cursor_x;
  previous_cursor_y = cursor_y;
  cursor_drawn = 1;
}

void gui_render(void) {
  uint32_t cursor_x = mouse_get_x();
  uint32_t cursor_y = mouse_get_y();

  gui_clamp_cursor(&cursor_x, &cursor_y);
  if (!cursor_drawn) {
    gui_render_full();
    return;
  }

  if (cursor_drawn) {
    gui_draw_desktop_region(previous_cursor_x, previous_cursor_y,
                            gui_icon_cursor.width * GUI_CURSOR_SCALE,
                            gui_icon_cursor.height * GUI_CURSOR_SCALE);
    gui_draw_terminal_region(previous_cursor_x, previous_cursor_y,
                             gui_icon_cursor.width * GUI_CURSOR_SCALE,
                             gui_icon_cursor.height * GUI_CURSOR_SCALE);
  }

  gui_draw_cursor(cursor_x, cursor_y);
  if (fb_uses_backbuffer()) {
    if (cursor_drawn)
      fb_present_rect(previous_cursor_x, previous_cursor_y,
                      gui_icon_cursor.width * GUI_CURSOR_SCALE,
                      gui_icon_cursor.height * GUI_CURSOR_SCALE);
    else
      fb_present();
    fb_present_rect(cursor_x, cursor_y,
                    gui_icon_cursor.width * GUI_CURSOR_SCALE,
                    gui_icon_cursor.height * GUI_CURSOR_SCALE);
  }
  previous_cursor_x = cursor_x;
  previous_cursor_y = cursor_y;
  cursor_drawn = 1;
}
