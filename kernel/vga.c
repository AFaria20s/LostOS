#include <stddef.h>
#include <stdint.h>

#include "../include/kstring.h"
#include "../include/io.h"
#include "../include/vga.h"

#define VGA_MEMORY 0xB8000
#define VGA_SCROLLBACK_HEIGHT 256

size_t t_row;
size_t t_column;
uint8_t t_color;
static uint16_t *t_buffer = (uint16_t *)VGA_MEMORY;
static uint16_t t_screen[VGA_HEIGHT][VGA_WIDTH];
static uint16_t t_scrollback[VGA_SCROLLBACK_HEIGHT][VGA_WIDTH];
static size_t t_scrollback_count = 0;
static size_t t_view_offset = 0;

static uint16_t t_blank_entry(void) {
  return vga_entry(' ', t_color);
}

static void t_clear_row(uint16_t row[VGA_WIDTH]) {
  for (size_t x = 0; x < VGA_WIDTH; x++)
    row[x] = t_blank_entry();
}

static void t_copy_row(uint16_t dest[VGA_WIDTH], const uint16_t src[VGA_WIDTH]) {
  for (size_t x = 0; x < VGA_WIDTH; x++)
    dest[x] = src[x];
}

static void t_render_view(void) {
  size_t total_lines = t_scrollback_count + VGA_HEIGHT;
  size_t start = 0;

  if (total_lines > VGA_HEIGHT + t_view_offset)
    start = total_lines - VGA_HEIGHT - t_view_offset;

  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    size_t line = start + y;
    const uint16_t *src;

    if (line < t_scrollback_count)
      src = t_scrollback[line];
    else
      src = t_screen[line - t_scrollback_count];

    for (size_t x = 0; x < VGA_WIDTH; x++)
      t_buffer[y * VGA_WIDTH + x] = src[x];
  }

  if (t_view_offset == 0)
    vga_update_cursor(t_column, t_row);
}

void t_scroll_to_bottom(void) {
  if (t_view_offset == 0)
    return;

  t_view_offset = 0;
  t_render_view();
}

void t_init(void) {
  t_row = 0;
  t_column = 0;
  t_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

  for (size_t y = 0; y < VGA_HEIGHT; y++)
    t_clear_row(t_screen[y]);
  t_scrollback_count = 0;
  t_view_offset = 0;
  t_render_view();
}

void t_setcolor(uint8_t color) { t_color = color; }

void t_putentryat(char c, uint8_t color, size_t x, size_t y) {
  if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
    return;

  t_scroll_to_bottom();
  t_screen[y][x] = vga_entry(c, color);
  t_buffer[y * VGA_WIDTH + x] = t_screen[y][x];
}

void t_scroll(void) {
  if (t_scrollback_count < VGA_SCROLLBACK_HEIGHT) {
    t_copy_row(t_scrollback[t_scrollback_count], t_screen[0]);
    t_scrollback_count++;
  } else {
    for (size_t y = 1; y < VGA_SCROLLBACK_HEIGHT; y++)
      t_copy_row(t_scrollback[y - 1], t_scrollback[y]);
    t_copy_row(t_scrollback[VGA_SCROLLBACK_HEIGHT - 1], t_screen[0]);
  }

  for (size_t y = 1; y < VGA_HEIGHT; y++)
    t_copy_row(t_screen[y - 1], t_screen[y]);

  t_clear_row(t_screen[VGA_HEIGHT - 1]);
  t_row = VGA_HEIGHT - 1;

  if (t_view_offset == 0)
    t_render_view();
}

void t_scroll_view_up(void) {
  if (t_view_offset >= t_scrollback_count)
    return;

  t_view_offset++;
  t_render_view();
}

void t_scroll_view_down(void) {
  if (t_view_offset == 0)
    return;

  t_view_offset--;
  t_render_view();
}

void t_clear(void) {
  for (size_t y = 0; y < VGA_HEIGHT; y++)
    t_clear_row(t_screen[y]);

  t_row = 0;
  t_column = 0;
  t_scrollback_count = 0;
  t_view_offset = 0;
  t_render_view();
}

void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
  outb(0x3D4, 0x0A);
  outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);

  outb(0x3D4, 0x0B);
  outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void vga_disable_cursor(void) {
  outb(0x3D4, 0x0A);
  outb(0x3D5, 0x20);
}

void vga_update_cursor(size_t col, size_t row) {
  uint16_t pos = (uint16_t)(row * VGA_WIDTH + col);

  outb(0x3D4, 0x0F);
  outb(0x3D5, (uint8_t)(pos & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void t_putchar(char c) {
  t_scroll_to_bottom();

  if (c == '\n') {
    t_column = 0;
    if (++t_row == VGA_HEIGHT)
      t_scroll();
    else
      vga_update_cursor(t_column, t_row);
    return;
  }

  t_putentryat(c, t_color, t_column, t_row);
  if (++t_column == VGA_WIDTH) {
    t_column = 0;
    if (++t_row == VGA_HEIGHT)
      t_scroll();
  }

  vga_update_cursor(t_column, t_row);
}

void t_write(const char *data, size_t size) {
  for (size_t i = 0; i < size; i++)
    t_putchar(data[i]);
}

void t_print_raw(const char *data) {
  t_write(data, k_strlen(data));
}

void t_print_padded(const char *text, int width) {
  int len = 0;
  t_print(text);
  while (text[len])
    len++;
  while (len++ < width)
    t_putchar(' ');
}

static void t_print_color_code(char code) {
  switch (code) {
  case '0':
    t_setcolor(vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_BLACK));
    break;
  case '1':
    t_setcolor(vga_entry_color(VGA_COLOR_BLUE, VGA_COLOR_BLACK));
    break;
  case '2':
    t_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    break;
  case '3':
    t_setcolor(vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
    break;
  case '4':
    t_setcolor(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
    break;
  case '5':
    t_setcolor(vga_entry_color(VGA_COLOR_MAGENTA, VGA_COLOR_BLACK));
    break;
  case '6':
    t_setcolor(vga_entry_color(VGA_COLOR_BROWN, VGA_COLOR_BLACK));
    break;
  case '7':
    t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    break;
  case '8':
    t_setcolor(vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK));
    break;
  case '9':
    t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
    break;
  case 'a':
    t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    break;
  case 'b':
    t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    break;
  case 'c':
    t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    break;
  case 'd':
    t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
    break;
  case 'e':
    t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
    break;
  case 'f':
    t_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    break;
  default:
    break;
  }
}

void t_print(const char *data) {
  while (*data) {
    if (*data == '$') {
      char next = *(data + 1);

      if (next == '\0') {
        t_putchar('$');
        break;
      }

      if ((next >= '0' && next <= '9') || (next >= 'a' && next <= 'f')) {
        t_print_color_code(next);
        data += 2;
        continue;
      }

      t_putchar('$');
      data++;
    } else {
      t_putchar(*data);
      data++;
    }
  }

  t_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));  // reset color
}