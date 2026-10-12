#include <stdint.h>

#include "drivers/fb.h"
#include "drivers/mouse.h"
#include "gui/font.h"
#include "gui/gui.h"
#include "gui/icons.h"
#include "gui/theme.h"
#include "mm/memory.h"
#include "terminal/terminal.h"

#define GUI_CURSOR_SCALE 1
#define GUI_TERMINAL_PADDING 16
#define GUI_TERMINAL_TITLE_HEIGHT 24
#define GUI_TERMINAL_LINE_GAP 2
#define GUI_WINDOW_CONTROL_SIZE 12
#define GUI_LAUNCHER_HEIGHT 30
#define GUI_RESIZE_HANDLE_SIZE 14
#define GUI_WINDOW_MIN_WIDTH 280
#define GUI_WINDOW_MIN_HEIGHT 180

static uint32_t previous_cursor_x;
static uint32_t previous_cursor_y;
static int cursor_drawn;

struct gui_window {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint32_t restore_x;
  uint32_t restore_y;
  uint32_t restore_width;
  uint32_t restore_height;
  int visible;
  int maximized;
};

static struct gui_window terminal_window;
static uint8_t previous_buttons;
static int start_menu_open;
static int dragging_window;
static int resizing_window;
static int32_t drag_offset_x;
static int32_t drag_offset_y;
static uint32_t damage_x;
static uint32_t damage_y;
static uint32_t damage_width;
static uint32_t damage_height;

static uint32_t gui_terminal_min_width(void) {
  return GUI_WINDOW_MIN_WIDTH;
}

static uint32_t gui_terminal_min_height(void) {
  return GUI_WINDOW_MIN_HEIGHT;
}

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

  layout.width = terminal_window.width;
  layout.height = terminal_window.height;
  layout.x = terminal_window.x;
  layout.y = terminal_window.y;
  layout.text_x = layout.x + GUI_TERMINAL_PADDING;
  layout.text_y = layout.y + GUI_TERMINAL_TITLE_HEIGHT + GUI_TERMINAL_PADDING;
  return layout;
}

static uint32_t gui_terminal_color(uint8_t vga_color) {
  switch (vga_color & 0x0F) {
    case TERMINAL_COLOR_BLACK: return gui_theme_default.background;
    case TERMINAL_COLOR_BLUE:
    case TERMINAL_COLOR_LIGHT_BLUE: return 0x007EB6FF;
    case TERMINAL_COLOR_GREEN:
    case TERMINAL_COLOR_LIGHT_GREEN: return 0x008BDB91;
    case TERMINAL_COLOR_CYAN:
    case TERMINAL_COLOR_BROWN:
    case TERMINAL_COLOR_LIGHT_BROWN: return gui_theme_default.accent;
    case TERMINAL_COLOR_RED:
    case TERMINAL_COLOR_LIGHT_RED: return 0x00F27A7A;
    case TERMINAL_COLOR_MAGENTA:
    case TERMINAL_COLOR_LIGHT_MAGENTA: return 0x00D48AF5;
    case TERMINAL_COLOR_DARK_GREY: return gui_theme_default.text_muted;
    case TERMINAL_COLOR_WHITE:
    case TERMINAL_COLOR_LIGHT_GREY: return gui_theme_default.text;
    case TERMINAL_COLOR_LIGHT_CYAN: return gui_theme_default.highlight;
    default: return gui_theme_default.text;
  }
}

static void gui_draw_terminal_region(uint32_t clip_x, uint32_t clip_y,
                                     uint32_t clip_width,
                                     uint32_t clip_height) {
  struct gui_terminal_layout layout = gui_terminal_layout();
  uint32_t content_right = layout.x + layout.width - GUI_TERMINAL_PADDING;
  uint32_t content_bottom = layout.y + layout.height - GUI_TERMINAL_PADDING;

  if (!terminal_window.visible)
    return;

  gui_fill_clipped(layout.x, layout.y, layout.width, layout.height,
                   gui_theme_default.border, clip_x, clip_y, clip_width,
                   clip_height);
  gui_fill_clipped(layout.x + 1, layout.y + 1, layout.width - 2,
                   layout.height - 2, gui_theme_default.surface, clip_x,
                   clip_y, clip_width, clip_height);
  gui_fill_clipped(layout.x + 1, layout.y + 1, layout.width - 2,
                   GUI_TERMINAL_TITLE_HEIGHT, gui_theme_default.background,
                   clip_x, clip_y, clip_width, clip_height);

  for (uint32_t row = 0; row < TERMINAL_HEIGHT; row++) {
    for (uint32_t column = 0; column < TERMINAL_WIDTH; column++) {
      uint16_t cell = t_get_cell(column, row);
      char character = (char)(cell & 0xFF);
      uint8_t color = (uint8_t)(cell >> 8);
      uint32_t character_x = layout.text_x + column * (GUI_FONT_WIDTH + 1);
      uint32_t character_y = layout.text_y +
                             row * (GUI_FONT_HEIGHT + GUI_TERMINAL_LINE_GAP);

      if (character_x + GUI_FONT_WIDTH > clip_x &&
          character_x < clip_x + clip_width &&
          character_y + GUI_FONT_HEIGHT > clip_y &&
          character_y < clip_y + clip_height &&
          character_x + GUI_FONT_WIDTH <= content_right &&
          character_y + GUI_FONT_HEIGHT <= content_bottom)
        gui_draw_char(character_x, character_y, character, 1,
                      gui_terminal_color(color));
    }
  }

  gui_draw_text(layout.x + GUI_TERMINAL_PADDING, layout.y + 8,
                "LostOS Terminal", 1, gui_theme_default.highlight);
  fb_fill_rect(layout.x + layout.width - 54, layout.y + 6,
               GUI_WINDOW_CONTROL_SIZE, GUI_WINDOW_CONTROL_SIZE,
               gui_theme_default.text_muted);
  fb_fill_rect(layout.x + layout.width - 36, layout.y + 6,
               GUI_WINDOW_CONTROL_SIZE, GUI_WINDOW_CONTROL_SIZE,
               gui_theme_default.accent);
  fb_fill_rect(layout.x + layout.width - 18, layout.y + 6,
               GUI_WINDOW_CONTROL_SIZE, GUI_WINDOW_CONTROL_SIZE, 0x00C64B4B);
  fb_fill_rect(layout.x + layout.width - GUI_RESIZE_HANDLE_SIZE,
               layout.y + layout.height - GUI_RESIZE_HANDLE_SIZE,
               GUI_RESIZE_HANDLE_SIZE, GUI_RESIZE_HANDLE_SIZE,
               gui_theme_default.text_muted);
  if (t_column < TERMINAL_WIDTH && t_row < TERMINAL_HEIGHT)
    if (layout.text_x + t_column * (GUI_FONT_WIDTH + 1) + GUI_FONT_WIDTH <=
        content_right &&
        layout.text_y + t_row * (GUI_FONT_HEIGHT + GUI_TERMINAL_LINE_GAP) +
        GUI_FONT_HEIGHT < content_bottom)
      fb_fill_rect(layout.text_x + t_column * (GUI_FONT_WIDTH + 1),
                   layout.text_y + t_row * (GUI_FONT_HEIGHT + GUI_TERMINAL_LINE_GAP) +
                   GUI_FONT_HEIGHT, GUI_FONT_WIDTH, 1, gui_theme_default.accent);
}

static void gui_draw_terminal(void) {
  gui_draw_terminal_region(0, 0, fb_get_width(), fb_get_height());
}

static void gui_draw_launcher_region(uint32_t clip_x, uint32_t clip_y,
                                     uint32_t clip_width,
                                     uint32_t clip_height) {
  uint32_t screen_height = fb_get_height();
  uint32_t launcher_y = screen_height > GUI_LAUNCHER_HEIGHT ?
                        screen_height - GUI_LAUNCHER_HEIGHT : 0;

  gui_fill_clipped(0, launcher_y, fb_get_width(), GUI_LAUNCHER_HEIGHT,
                   gui_theme_default.surface, clip_x, clip_y, clip_width,
                   clip_height);
  gui_fill_clipped(8, launcher_y + 6, 76, 18, gui_theme_default.border,
                   clip_x, clip_y, clip_width, clip_height);
  gui_draw_text(16, launcher_y + 11, "Apps", 1, gui_theme_default.text);
  if (start_menu_open) {
    gui_fill_clipped(8, launcher_y - 58, 164, 54, gui_theme_default.border,
                     clip_x, clip_y, clip_width, clip_height);
    gui_fill_clipped(10, launcher_y - 56, 160, 50, gui_theme_default.surface,
                     clip_x, clip_y, clip_width, clip_height);
    gui_draw_text(20, launcher_y - 42, "Terminal", 1,
                  gui_theme_default.highlight);
  }
}

static void gui_draw_launcher(void) {
  gui_draw_launcher_region(0, 0, fb_get_width(), fb_get_height());
}

static int gui_point_in(uint32_t x, uint32_t y, uint32_t rect_x,
                        uint32_t rect_y, uint32_t width, uint32_t height) {
  return x >= rect_x && y >= rect_y && x < rect_x + width &&
         y < rect_y + height;
}

static void gui_set_damage(uint32_t first_x, uint32_t first_y,
                           uint32_t first_width, uint32_t first_height,
                           uint32_t second_x, uint32_t second_y,
                           uint32_t second_width, uint32_t second_height) {
  uint32_t left = first_x < second_x ? first_x : second_x;
  uint32_t top = first_y < second_y ? first_y : second_y;
  uint64_t first_right = (uint64_t)first_x + first_width;
  uint64_t second_right = (uint64_t)second_x + second_width;
  uint64_t first_bottom = (uint64_t)first_y + first_height;
  uint64_t second_bottom = (uint64_t)second_y + second_height;
  uint64_t right = first_right > second_right ? first_right : second_right;
  uint64_t bottom = first_bottom > second_bottom ? first_bottom : second_bottom;

  damage_x = left;
  damage_y = top;
  damage_width = (uint32_t)(right - left);
  damage_height = (uint32_t)(bottom - top);
}

static void gui_maximize_terminal(void) {
  if (!terminal_window.maximized) {
    terminal_window.restore_x = terminal_window.x;
    terminal_window.restore_y = terminal_window.y;
    terminal_window.restore_width = terminal_window.width;
    terminal_window.restore_height = terminal_window.height;
    terminal_window.x = 8;
    terminal_window.y = 8;
    terminal_window.width = fb_get_width() > 16 ? fb_get_width() - 16 : 1;
    terminal_window.height = fb_get_height() > GUI_LAUNCHER_HEIGHT + 16 ?
                             fb_get_height() - GUI_LAUNCHER_HEIGHT - 16 : 1;
    terminal_window.maximized = 1;
  } else {
    terminal_window.x = terminal_window.restore_x;
    terminal_window.y = terminal_window.restore_y;
    terminal_window.width = terminal_window.restore_width;
    terminal_window.height = terminal_window.restore_height;
    terminal_window.maximized = 0;
  }
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
  terminal_window.width = TERMINAL_WIDTH * (GUI_FONT_WIDTH + 1) +
                          GUI_TERMINAL_PADDING * 2;
  terminal_window.height = TERMINAL_HEIGHT *
                           (GUI_FONT_HEIGHT + GUI_TERMINAL_LINE_GAP) +
                           GUI_TERMINAL_PADDING * 2 + GUI_TERMINAL_TITLE_HEIGHT;
  terminal_window.x = fb_get_width() > terminal_window.width ?
                      (fb_get_width() - terminal_window.width) / 2 : 0;
  terminal_window.y = 56;
  terminal_window.visible = 1;
  terminal_window.maximized = 0;
  previous_buttons = 0;
  start_menu_open = 0;
  dragging_window = 0;
  resizing_window = 0;
  if (bytes <= 0xFFFFFFFFU) {
    backbuffer = kmalloc((size_t)bytes);
    if (backbuffer && fb_set_backbuffer(backbuffer, fb_get_pitch()) != 0)
      backbuffer = 0;
  }
}

int gui_handle_pointer(uint32_t x, uint32_t y, uint8_t buttons) {
  struct gui_terminal_layout layout = gui_terminal_layout();
  int left_pressed = (buttons & MOUSE_BUTTON_LEFT) &&
                     !(previous_buttons & MOUSE_BUTTON_LEFT);
  int changed = 0;
  uint32_t launcher_y = fb_get_height() > GUI_LAUNCHER_HEIGHT ?
                        fb_get_height() - GUI_LAUNCHER_HEIGHT : 0;

  if (dragging_window && !(buttons & MOUSE_BUTTON_LEFT))
    dragging_window = 0;
  if (resizing_window && !(buttons & MOUSE_BUTTON_LEFT))
    resizing_window = 0;
  if (dragging_window && (buttons & MOUSE_BUTTON_LEFT)) {
    uint32_t old_x = terminal_window.x;
    uint32_t old_y = terminal_window.y;
    int32_t next_x = (int32_t)x - drag_offset_x;
    int32_t next_y = (int32_t)y - drag_offset_y;
    if (next_x < 0) next_x = 0;
    if (next_y < 0) next_y = 0;
    terminal_window.x = (uint32_t)next_x;
    terminal_window.y = (uint32_t)next_y;
    gui_set_damage(old_x, old_y, terminal_window.width, terminal_window.height,
                   terminal_window.x, terminal_window.y, terminal_window.width,
                   terminal_window.height);
    changed = 1;
  } else if (resizing_window && (buttons & MOUSE_BUTTON_LEFT)) {
    uint32_t old_width = terminal_window.width;
    uint32_t old_height = terminal_window.height;
    uint32_t min_width = gui_terminal_min_width();
    uint32_t min_height = gui_terminal_min_height();
    uint32_t max_width = fb_get_width() > terminal_window.x ?
                         fb_get_width() - terminal_window.x : min_width;
    uint32_t max_height = fb_get_height() > GUI_LAUNCHER_HEIGHT + terminal_window.y ?
                          fb_get_height() - GUI_LAUNCHER_HEIGHT - terminal_window.y :
                          min_height;
    uint32_t next_width = x >= terminal_window.x ? x - terminal_window.x + 1 : 0;
    uint32_t next_height = y >= terminal_window.y ? y - terminal_window.y + 1 : 0;

    if (next_width < min_width) next_width = min_width;
    if (next_height < min_height) next_height = min_height;
    if (next_width > max_width) next_width = max_width;
    if (next_height > max_height) next_height = max_height;
    terminal_window.width = next_width;
    terminal_window.height = next_height;
    gui_set_damage(terminal_window.x, terminal_window.y, old_width, old_height,
                   terminal_window.x, terminal_window.y, next_width, next_height);
    changed = 1;
  } else if (left_pressed && gui_point_in(x, y, 8, launcher_y + 6, 76, 18)) {
    start_menu_open = !start_menu_open;
    changed = 1;
  } else if (left_pressed && start_menu_open &&
             gui_point_in(x, y, 10, launcher_y - 56, 160, 50)) {
    terminal_window.visible = 1;
    start_menu_open = 0;
    changed = 1;
  } else if (left_pressed && terminal_window.visible &&
             !terminal_window.maximized &&
             gui_point_in(x, y, layout.x + layout.width - GUI_RESIZE_HANDLE_SIZE,
                          layout.y + layout.height - GUI_RESIZE_HANDLE_SIZE,
                          GUI_RESIZE_HANDLE_SIZE, GUI_RESIZE_HANDLE_SIZE)) {
    resizing_window = 1;
  } else if (left_pressed && terminal_window.visible &&
             gui_point_in(x, y, layout.x + layout.width - 18, layout.y + 6,
                          GUI_WINDOW_CONTROL_SIZE, GUI_WINDOW_CONTROL_SIZE)) {
    terminal_window.visible = 0;
    changed = 1;
  } else if (left_pressed && terminal_window.visible &&
             gui_point_in(x, y, layout.x + layout.width - 36, layout.y + 6,
                          GUI_WINDOW_CONTROL_SIZE, GUI_WINDOW_CONTROL_SIZE)) {
    gui_maximize_terminal();
    changed = 1;
  } else if (left_pressed && terminal_window.visible &&
             gui_point_in(x, y, layout.x + layout.width - 54, layout.y + 6,
                          GUI_WINDOW_CONTROL_SIZE, GUI_WINDOW_CONTROL_SIZE)) {
    terminal_window.visible = 0;
    changed = 1;
  } else if (left_pressed && terminal_window.visible && !terminal_window.maximized &&
             gui_point_in(x, y, layout.x, layout.y, layout.width,
                          GUI_TERMINAL_TITLE_HEIGHT)) {
    dragging_window = 1;
    drag_offset_x = (int32_t)x - (int32_t)layout.x;
    drag_offset_y = (int32_t)y - (int32_t)layout.y;
  }

  previous_buttons = buttons;
  return changed && (dragging_window || resizing_window) ? 2 : changed;
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
  gui_draw_launcher();
  gui_draw_terminal();
  gui_draw_cursor(cursor_x, cursor_y);
  if (fb_uses_backbuffer())
    fb_present();
  previous_cursor_x = cursor_x;
  previous_cursor_y = cursor_y;
  cursor_drawn = 1;
}

void gui_render_damage(void) {
  uint32_t cursor_x = mouse_get_x();
  uint32_t cursor_y = mouse_get_y();

  gui_clamp_cursor(&cursor_x, &cursor_y);
  gui_draw_desktop_region(damage_x, damage_y, damage_width, damage_height);
  gui_draw_launcher_region(damage_x, damage_y, damage_width, damage_height);
  gui_draw_terminal_region(damage_x, damage_y, damage_width, damage_height);
  gui_draw_cursor(cursor_x, cursor_y);
  if (fb_uses_backbuffer())
    fb_present_rect(damage_x, damage_y, damage_width, damage_height);
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
    gui_draw_launcher_region(previous_cursor_x, previous_cursor_y,
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
