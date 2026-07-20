#ifndef VGA_H
#define VGA_H

#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

enum vga_color {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
  return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
  return (uint16_t)uc | (uint16_t)color << 8;
}

extern size_t t_row;
extern size_t t_column;
extern uint8_t t_color;

void t_putchar(char c);
void t_putentryat(char c, uint8_t color, size_t x, size_t y);
void t_init(void);
void t_print(const char *data);
void t_print_raw(const char *data);
void t_setcolor(uint8_t color);
void t_scroll(void);
void t_scroll_view_up(void);
void t_scroll_view_down(void);
void t_scroll_to_bottom(void);
void t_print_padded(const char *text, int width);
void t_force_redraw(void);
void t_clear(void);

// Controls the text cursor.
void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end);
void vga_disable_cursor(void);
void vga_update_cursor(size_t col, size_t row);

#endif
