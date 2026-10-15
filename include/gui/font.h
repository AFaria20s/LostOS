#ifndef GUI_FONT_H
#define GUI_FONT_H

#include <stdint.h>

#define GUI_FONT_WIDTH 5
#define GUI_FONT_HEIGHT 7

void gui_draw_char(uint32_t x, uint32_t y, char character, uint32_t scale,
                   uint32_t color);
void gui_draw_text(uint32_t x, uint32_t y, const char *text, uint32_t scale,
                   uint32_t color);
uint32_t gui_text_width(const char *text, uint32_t scale);

#endif
