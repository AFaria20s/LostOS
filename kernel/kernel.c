#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../include/io.h"
#include "../include/kstring.h"
#include "../include/memory.h"
#include "../include/shell.h"
#include "../include/vga.h"

extern void idt_init(void);
extern void gdt_init(void);

static const char *old_boot_logo =
"  ___           _    ___    _  _       ___  ____  \n"
" / _ \\  __  __ / |  / _ \\  | || |     / _ \\/ ___| \n"
"| | | | \\ \\/ / | | | (_) | | || |_   | | | \\___ \\  \n"
"| |_| |  >  <  | |  \\__, | |__   _|  | |_| |___) | \n"
" \\___/  /_/\\_\\ |_|    /_/     |_|     \\___/|____/  \n";

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

#define VGA_MEMORY 0xB8000
#define VGA_SCROLLBACK_HEIGHT 256

size_t t_row;
size_t t_column;
uint8_t t_color;
uint16_t *t_buffer = (uint16_t *)VGA_MEMORY;
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
  // Write a character at (x,y) with color
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
  // Clear the screen and reset cursor
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
  // Put a character into the buffer and handle newlines and wrapping
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

  t_write(data, k_strlen(data));
}

static void print_boot_status(const char *label, int ok) {
  t_print(ok ? "$a[ OK  ]$f " : "$c[ FAIL ]$f ");
  t_print_raw(label);
  t_putchar('\n');
}

static void welcome_screen(void) {
  t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
  t_putchar('\n');
  t_print_raw(old_boot_logo);

  t_print("\n");
  t_print("$f0x194 OS $8/ $b32-bit bare metal shell$f\n");
  t_print("$8Build: multiboot, VGA text, PS/2 keyboard, kernel heap$f\n\n");

  t_print("$bBoot status$f\n");
  print_boot_status("GDT loaded", 1);
  print_boot_status("IDT loaded", 1);
  print_boot_status("Keyboard IRQ ready", 1);
  print_boot_status("Kernel heap ready", memory_is_ready());

  t_print("\n$bUseful commands$f\n");
  t_print("help        list commands\n");
  t_print("mem         inspect kernel memory\n");
  t_print("mem test -d run detailed heap test\n");
  t_print("layout      show or change keyboard layout\n");
  t_print("history     show command history\n\n");
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
  // Kernel entry point. Initialize subsystems and start shell
  t_init();
  memory_init(multiboot_magic, multiboot_info_addr);
  gdt_init();
  idt_init();
  vga_enable_cursor(14, 15);

  welcome_screen();

  shell_prompt();

  for(;;);
}
