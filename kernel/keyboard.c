#include <stdint.h>

#include "../include/vga.h"

#define KEYBOARD_PORT 0x60

// read byte at given port
static inline uint8_t inb(uint16_t port) {
  uint8_t val;
  __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
  return val;
}

// write byte at given port
static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// normal keyboard symbols
static const char scancode_normal[] = {
    0,   0,   '1',  '2',  '3',  '4', '5', '6',  '7', '8', '9', '0',
    '-', '=', '\b', '\t', 'q',  'w', 'e', 'r',  't', 'y', 'u', 'i',
    'o', 'p', '[',  ']',  '\n', 0,   'a', 's',  'd', 'f', 'g', 'h',
    'j', 'k', 'l',  ';',  '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm',  ',',  '.',  '/', 0,   '*',  0,   ' '};

// shift-activated keyboard symbols
static const char scancode_shift[] = {
    0, 0, '!','"','#','$','%','&','/','(',')','=','?','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','[',']','\n',
    0,'A','S','D','F','G','H','J','K','L',';','\'','`',
    0,'\\','Z','X','C','V','B','N','M',';',':','_',0,
    '*',0,' '
};

static int shift_pressed = 0;
static int caps_lock = 0;

void keyboard_handler(void) {
  uint8_t scancode = inb(KEYBOARD_PORT);

  if(scancode==0x2A || scancode==0x36) shift_pressed = 1; // shift pressed
  if(scancode==(0x2A | 0x80) || scancode==(0x36 | 0x80)) shift_pressed = 0; // shift released

  if (scancode == 0x3A) caps_lock = !caps_lock; // caps lock

  // if not released -> 0x80
  else if (!(scancode & 0x80)) {
    if (scancode < sizeof(scancode_normal)) {
      char c;
      int use_upper = (caps_lock ^ shift_pressed);

      if (shift_pressed) {
        c = scancode_shift[scancode];
      } else {
        c = scancode_normal[scancode];
      }

      // caps lock for letters only
      if (caps_lock && !shift_pressed && c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
      } else if (caps_lock && shift_pressed && c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
      }

      if (c) t_putchar(c);
    }
  }

  outb(0x20, 0x20);
}
