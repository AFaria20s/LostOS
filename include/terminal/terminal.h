#ifndef TERMINAL_TERMINAL_H
#define TERMINAL_TERMINAL_H

#include <stddef.h>
#include <stdint.h>

/* The GUI terminal is independent from the 80x25 VGA hardware surface. */
#ifdef GUI
#define TERMINAL_WIDTH 120
#define TERMINAL_HEIGHT 40
#else
#define TERMINAL_WIDTH 80
#define TERMINAL_HEIGHT 25
#endif

enum terminal_color {
  TERMINAL_COLOR_BLACK = 0,
  TERMINAL_COLOR_BLUE = 1,
  TERMINAL_COLOR_GREEN = 2,
  TERMINAL_COLOR_CYAN = 3,
  TERMINAL_COLOR_RED = 4,
  TERMINAL_COLOR_MAGENTA = 5,
  TERMINAL_COLOR_BROWN = 6,
  TERMINAL_COLOR_LIGHT_GREY = 7,
  TERMINAL_COLOR_DARK_GREY = 8,
  TERMINAL_COLOR_LIGHT_BLUE = 9,
  TERMINAL_COLOR_LIGHT_GREEN = 10,
  TERMINAL_COLOR_LIGHT_CYAN = 11,
  TERMINAL_COLOR_LIGHT_RED = 12,
  TERMINAL_COLOR_LIGHT_MAGENTA = 13,
  TERMINAL_COLOR_LIGHT_BROWN = 14,
  TERMINAL_COLOR_WHITE = 15,
};

static inline uint8_t terminal_color_pair(enum terminal_color fg,
                                          enum terminal_color bg) {
  return fg | bg << 4;
}

static inline uint16_t terminal_cell(unsigned char character, uint8_t color) {
  return (uint16_t)character | (uint16_t)color << 8;
}

extern size_t t_row;
extern size_t t_column;
extern uint8_t t_color;

void t_putchar(char c);
void t_putentryat(char c, uint8_t color, size_t x, size_t y);
void t_init(void);
void t_print(const char *data);
void t_print_raw(const char *data);
void t_write(const char *data, size_t size);
void t_set_color_code(char code);
void t_setcolor(uint8_t color);
void t_scroll(void);
void t_scroll_view_up(void);
void t_scroll_view_down(void);
void t_scroll_to_bottom(void);
void t_print_padded(const char *text, int width);
void t_force_redraw(void);
void t_clear(void);
uint16_t t_get_cell(size_t x, size_t y);
int t_take_dirty(void);
void t_capture_begin(char *buffer, size_t capacity);
size_t t_capture_end(void);

#endif
