#ifndef GUI_ICONS_H
#define GUI_ICONS_H

#include <stdint.h>

struct gui_icon {
  const char *const *rows;
  uint32_t width;
  uint32_t height;
};

/*
 * Define an icon with readable pixel-art rows and its row width.  '.' is
 * transparent; '#', '+' and '@' use the shadow, light and accent colours
 * passed to gui_draw_icon.
 */
#define GUI_DEFINE_ICON(name, icon_width, ...)                               \
  static const char *const name##_rows[] = { __VA_ARGS__ };                  \
  const struct gui_icon name = {                                             \
    name##_rows, icon_width,                                                 \
    sizeof(name##_rows) / sizeof(name##_rows[0])                             \
  }

extern const struct gui_icon gui_icon_cursor;

void gui_draw_icon(const struct gui_icon *icon, uint32_t x, uint32_t y,
                   uint32_t scale, uint32_t shadow, uint32_t light,
                   uint32_t accent);

#endif
