#ifndef GUI_H
#define GUI_H

void gui_init(void);
void gui_render(void);
void gui_render_full(void);
void gui_render_damage(void);
int gui_handle_pointer(uint32_t x, uint32_t y, uint8_t buttons);

#endif
