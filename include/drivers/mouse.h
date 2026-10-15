#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

int mouse_init(void);
void mouse_handler(void);
void mouse_poll(void);
void mouse_set_bounds(uint32_t width, uint32_t height);
uint32_t mouse_get_x(void);
uint32_t mouse_get_y(void);
uint8_t mouse_get_buttons(void);
int mouse_has_changed(void);

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

#endif
