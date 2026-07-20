#ifndef EDITOR_H
#define EDITOR_H

#include <stdint.h>

#define EDITOR_MAX_LINES    256
#define EDITOR_LINE_LENGTH  128

typedef enum {
    EDITOR_MODE_COMMAND,
    EDITOR_MODE_INSERT
} editor_mode_t;

struct editor_line {
    char text[EDITOR_LINE_LENGTH];
    int length;
    int h_scroll;
};

struct editor_state {
    struct editor_line lines[EDITOR_MAX_LINES];
    int line_count;

    int cursor_x;
    int cursor_y;
    int scroll_offset;

    char filepath[64];
    editor_mode_t mode;
    int modified;
};

#endif