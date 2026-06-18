#include <stdint.h>

#include "../include/io.h"
#include "../include/keyboard.h"
#include "../include/shell.h"

#define KEYBOARD_PORT 0x60

static const char scancode_normal[] = {
    0,    0,    '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',
    '\'', (char)0xAE, '\b', '\t', 'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
    'o',  'p',  '+',  '`',  '\n', 0,    'a',  's',  'd',  'f',  'g',  'h',
    'j',  'k',  'l',  (char)0x87, (char)0xA7, (char)0x15, 0,    '~',  'z',  'x',  'c',  'v',
    'b',  'n',  'm',  ',',  '.',  '-',  0,    '*',  0,    ' '
};

static const char scancode_shift[] = {
    0,    0,    '!',  '"',  '#',  '$',  '%',  '&',  '/',  '(',  ')',  '=',
    '?',  (char)0xAF, '\b', '\t', 'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
    'O',  'P',  '*',  '^',  '\n', 0,    'A',  'S',  'D',  'F',  'G',  'H',
    'J',  'K',  'L',  (char)0x80, (char)0xA6, '|', 0,    '^',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  ';',  ':',  '_',  0,    '*',  0,    ' '
};

static int shift_pressed = 0;
static int caps_lock = 0;

// Tracks extended scancodes.
static int extended = 0;

void keyboard_handler(void) {
  uint8_t scancode = inb(KEYBOARD_PORT);

  if (scancode == 0xE0) {
    extended = 1;
    outb(0x20, 0x20);
    return;
  }

  if (extended) {
    extended = 0;
    if (!(scancode & 0x80)) {
      switch (scancode) {
        case 0x4B: shell_input(KEY_LEFT);   break;
        case 0x4D: shell_input(KEY_RIGHT);  break;
        case 0x48: shell_input(KEY_UP);     break;
        case 0x50: shell_input(KEY_DOWN);   break;
        case 0x53: shell_input(KEY_DELETE); break;
        case 0x49: shell_input(KEY_PGUP);   break;
        case 0x51: shell_input(KEY_PGDOWN); break;
      }
    }
    outb(0x20, 0x20);
    return;
  }

  if (scancode == 0x2A || scancode == 0x36) shift_pressed = 1;
  if (scancode == (0x2A | 0x80) || scancode == (0x36 | 0x80)) shift_pressed = 0;

  if (scancode == 0x3A) {
    caps_lock = !caps_lock;
  } else if (!(scancode & 0x80)) {
    char c = 0;

    if (scancode == 0x48) {
      shell_input(KEY_UP);
      outb(0x20, 0x20);
      return;
    } else if (scancode == 0x50) {
      shell_input(KEY_DOWN);
      outb(0x20, 0x20);
      return;
    } else if (scancode == 0x49) {
      shell_input(KEY_PGUP);
      outb(0x20, 0x20);
      return;
    } else if (scancode == 0x51) {
      shell_input(KEY_PGDOWN);
      outb(0x20, 0x20);
      return;
    } else if (scancode == 0x56) {
      c = shift_pressed ? '>' : '<';
    } else if (scancode < sizeof(scancode_normal)) {
      c = shift_pressed ? scancode_shift[scancode] : scancode_normal[scancode];
    }

    if (caps_lock && !shift_pressed && c >= 'a' && c <= 'z') {
      c = c - 'a' + 'A';
    } else if (caps_lock && shift_pressed && c >= 'A' && c <= 'Z') {
      c = c - 'A' + 'a';
    }

    if (c) shell_input(c);
  }

  outb(0x20, 0x20);
}
