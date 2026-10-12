#include "drivers/fb.h"
#include "gui/icons.h"

GUI_DEFINE_ICON(gui_icon_cursor, 11,
  ".....#.....",
  "....#+#....",
  "...#@@@#...",
  "..#@@@@@#..",
  ".#@@@@@@@#.",
  "#@@@@@@@@@#",
  ".##@@@@@##.",
  "...#@@@#...",
  "...#@@@#...",
  "...#@@@#...",
  "...#@@@#...",
  "..#@@@@@#..",
  "...#####..."
);

void gui_draw_icon(const struct gui_icon *icon, uint32_t x, uint32_t y,
                   uint32_t scale, uint32_t shadow, uint32_t light,
                   uint32_t accent) {
  if (!icon || !icon->rows || scale == 0)
    return;

  for (uint32_t row = 0; row < icon->height; row++) {
    for (uint32_t column = 0; column < icon->width; column++) {
      char pixel = icon->rows[row][column];
      if (pixel == '#')
        fb_fill_rect(x + column * scale, y + row * scale, scale, scale, shadow);
      else if (pixel == '+')
        fb_fill_rect(x + column * scale, y + row * scale, scale, scale, light);
      else if (pixel == '@')
        fb_fill_rect(x + column * scale, y + row * scale, scale, scale, accent);
    }
  }
}
