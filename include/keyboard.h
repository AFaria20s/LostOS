#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_LEFT    0x100
#define KEY_RIGHT   0x101
#define KEY_DELETE  0x102
#define KEY_PGUP    0x103
#define KEY_PGDOWN  0x104
#define KEY_UP      0x105
#define KEY_DOWN    0x106
#define KEY_SCROLL_UP    0x107
#define KEY_SCROLL_DOWN  0x108

void keyboard_handler(void);
int keyboard_is_ready(void);
int keyboard_set_layout(const char *name);
const char *keyboard_get_layout(void);

#endif
