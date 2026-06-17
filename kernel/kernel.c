#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../include/vga.h"

extern void idt_init(void);
extern void gdt_init(void);

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

size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len])
    len++;
  return len;
}

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

size_t t_row;
size_t t_column;
uint8_t t_color;
uint16_t *t_buffer = (uint16_t *)VGA_MEMORY;

void t_init(void) {
  t_row = 0;
  t_column = 0;
  t_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      t_buffer[index] = vga_entry(' ', t_color);
    }
  }
}

void t_setcolor(uint8_t color) { t_color = color; }

void t_putentryat(char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  t_buffer[index] = vga_entry(c, color);
}

void t_putchar(char c) {
  if (c == '\n') {
    t_column = 0;
    t_row++;
    return;
  }

  t_putentryat(c, t_color, t_column, t_row);
  if (++t_column == VGA_WIDTH) {
    t_column = 0;
    if (++t_row == VGA_HEIGHT)
      t_row = 0;
  }
}

void t_write(const char *data, size_t size) {
  for (size_t i = 0; i < size; i++)
    t_putchar(data[i]);
}

void t_print(const char *data) {
  while (*data) {
    if (*data == '$') {
      data++;
      switch (*data) {
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
        t_putchar('$');
        t_putchar(*data);
        break;
      }
      data++;
    } else {
      t_putchar(*data);
      data++;
    }
  }

  t_write(data, strlen(data));
}

void kernel_main(void) {
  t_init();
  gdt_init();
  idt_init();

  t_setcolor(vga_entry_color(VGA_COLOR_MAGENTA, VGA_COLOR_BLACK));
  t_print("\n");
  t_print("  ___           _    ___    _  _       ___  ____  \n");
  t_print(" / _ \\  __  __ / |  / _ \\  | || |     / _ \\/ ___| \n");
  t_print("| | | | \\ \\/ / | | | (_) | | || |_   | | | \\___ \\  \n");
  t_print("| |_| |  >  <  | |  \\__, | |__   _|  | |_| |___) | \n");
  t_print(" \\___/  /_/\\_\\ |_|    /_/     |_|     \\___/|____/  \n");

  t_print("\n");
  t_print("$1Welcome $2to $c0x194 OS\n");

  for(;;); // infinite kernel loop
}
