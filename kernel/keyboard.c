#include <stdint.h>

#include "../include/io.h"
#include "../include/keyboard.h"
#include "../include/shell.h"
#include "../include/keyboard_layouts.h"

#define KEYBOARD_PORT 0x60
#define PIC_EOI       0x20

// PS/2 scancode constants
#define SCANCODE_ESCAPE      0x01
#define SCANCODE_BACKSPACE   0x0E
#define SCANCODE_TAB         0x0F
#define SCANCODE_ENTER       0x1C
#define SCANCODE_LSHIFT      0x2A
#define SCANCODE_RSHIFT      0x36
#define SCANCODE_CAPSLOCK    0x3A
#define SCANCODE_EXTENDED    0xE0
#define SCANCODE_RELEASE     0x80

// Extended scancode mappings (0xE0 prefix)
#define EXT_LEFT             0x4B
#define EXT_RIGHT            0x4D
#define EXT_UP               0x48
#define EXT_DOWN             0x50
#define EXT_DELETE           0x53
#define EXT_PGUP             0x49
#define EXT_PGDOWN           0x51
#define EXT_RALT             0x38

// Keyboard state
static const struct layout *cur_layout = &layouts[0];
static int shift_pressed = 0;
static int caps_lock = 0;
static int altgr_pressed = 0;
static int extended = 0;

int keyboard_is_ready(void) {
  return cur_layout != 0 &&
         cur_layout->name != 0 &&
         cur_layout->normal != 0 &&
         cur_layout->shift != 0 &&
         cur_layout->size > 0;
}

static void send_eoi(void) {
  outb(PIC_EOI, PIC_EOI);
}

static char translate_scancode(uint8_t scancode) {
  if ((size_t)scancode >= cur_layout->size)
    return 0;

  if (altgr_pressed && cur_layout->altgr) {
    char c = cur_layout->altgr[scancode];
    if (c)
      return c;
  }

  char c = shift_pressed ? cur_layout->shift[scancode] : cur_layout->normal[scancode];

  if (caps_lock && !shift_pressed && c >= 'a' && c <= 'z')
    return c - 'a' + 'A';
  if (caps_lock && shift_pressed && c >= 'A' && c <= 'Z')
    return c - 'A' + 'a';

  return c;
}

static void handle_extended_key(uint8_t scancode) {
  int is_release = (scancode & SCANCODE_RELEASE) != 0;
  scancode &= ~SCANCODE_RELEASE;

  if (!is_release) {
    switch (scancode) {
      case EXT_LEFT:    shell_input(KEY_LEFT);    break;
      case EXT_RIGHT:   shell_input(KEY_RIGHT);   break;
      case EXT_UP:
        shell_input(shift_pressed ? KEY_SCROLL_UP : KEY_UP);
        break;
      case EXT_DOWN:
        shell_input(shift_pressed ? KEY_SCROLL_DOWN : KEY_DOWN);
        break;
      case EXT_DELETE:  shell_input(KEY_DELETE);  break;
      case EXT_PGUP:    shell_input(KEY_PGUP);    break;
      case EXT_PGDOWN:  shell_input(KEY_PGDOWN);  break;
      case EXT_RALT:    altgr_pressed = 1;        break;
    }
  } else {
    if (scancode == EXT_RALT)
      altgr_pressed = 0;
  }
}

void keyboard_handler(void) {
  uint8_t scancode = inb(KEYBOARD_PORT);

  if (scancode == SCANCODE_EXTENDED) {
    extended = 1;
    send_eoi();
    return;
  }

  if (extended) {
    handle_extended_key(scancode);
    extended = 0;
    send_eoi();
    return;
  }

  uint8_t key = scancode & ~SCANCODE_RELEASE;
  int is_release = (scancode & SCANCODE_RELEASE) != 0;

  if (key == SCANCODE_LSHIFT || key == SCANCODE_RSHIFT) {
    shift_pressed = !is_release;
    send_eoi();
    return;
  }

  if (key == SCANCODE_CAPSLOCK && !is_release) {
    caps_lock = !caps_lock;
    send_eoi();
    return;
  }

  if (is_release) {
    send_eoi();
    return;
  }

  char c = translate_scancode(key);

  if (c)
    shell_input(c);

  send_eoi();
}

int keyboard_set_layout(const char *name) {
  const struct layout *layout = layout_find_by_name(name);
  if (!layout)
    return -1;
  cur_layout = layout;
  return 0;
}

const char *keyboard_get_layout(void) {
  return cur_layout->name;
}
