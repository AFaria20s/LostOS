#ifndef GUI_THEME_H
#define GUI_THEME_H

#include <stdint.h>

struct gui_theme {
  uint32_t background;
  uint32_t surface;
  uint32_t border;
  uint32_t text;
  uint32_t text_muted;
  uint32_t shadow;
  uint32_t highlight;
  uint32_t accent;
};

extern const struct gui_theme gui_theme_default;

#endif
