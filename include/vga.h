#ifndef VGA_H
#define VGA_H

#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

extern size_t t_row;
extern size_t t_column;
extern uint8_t t_color;

void t_putchar(char c);
void t_putentryat(char c, uint8_t color, size_t x, size_t y);
void t_print(const char *data);
void t_setcolor(uint8_t color);

// Clears the screen.
void t_clear(void);

// Controls the text cursor.
void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end);
void vga_disable_cursor(void);
void vga_update_cursor(size_t col, size_t row);

#endif
