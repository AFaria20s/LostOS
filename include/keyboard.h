#ifndef KEYBOARD_H
#define KEYBOARD_H

// Special keys sent to the shell.
#define KEY_LEFT    0x100
#define KEY_RIGHT   0x101
#define KEY_DELETE  0x102
#define KEY_PGUP    0x103
#define KEY_PGDOWN  0x104
#define KEY_UP      0x105
#define KEY_DOWN    0x106

void keyboard_handler(void);

#endif
