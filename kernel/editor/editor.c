#include "editor/editor.h"
#include "drivers/vga.h"
#include "mm/memory.h"
#include "lib/kstring.h"
#include "drivers/keyboard.h"
#include "fs/vfs.h"
#include "shell/shell.h"

static struct editor_state state;
static size_t saved_row, saved_column;
static int editor_active = 0;

static char command_buffer[16];
static int command_len = 0;
static int in_command_line = 0;

static void editor_save(void);
static void editor_load_from_file(struct vfs_file *file);

static uint8_t load_buf[EDITOR_MAX_LINES * EDITOR_LINE_LENGTH];

void editor_open(const char *path) {
    // save cursor position
    saved_row = t_row;
    saved_column = t_column;

    // initialize editor state
    state.line_count = 1;
    state.lines[0].length = 0;
    state.lines[0].h_scroll = 0;

    state.cursor_x = 0;
    state.cursor_y = 0;
    state.scroll_offset = 0;
    state.mode = EDITOR_MODE_COMMAND;
    state.modified = 0;

    k_strcp(state.filepath, path);

    struct vfs_file file;
    if (vfs_open(path, &file))
        editor_load_from_file(&file);

    editor_active = 1;
    editor_render();
}

void editor_render(void) {
    uint16_t *vga = (uint16_t *)0xB8000;
    uint8_t color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    // clean screen
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = vga_entry(' ', color);

    // draw visible lines
    int visible_lines = VGA_HEIGHT - 1;
    for (int i = 0; i < visible_lines; i++) {
        int line_index = state.scroll_offset + i;
        if (line_index >= state.line_count)
            break;

        struct editor_line *line = &state.lines[line_index];
        int start = line->h_scroll;
        int end = start + VGA_WIDTH;
        if (end > line->length)
            end = line->length;

        for (int x = start; x < end; x++)
            vga[i * VGA_WIDTH + (x - start)] = vga_entry(line->text[x], color);
    }

    // status bar
    int status_row = VGA_HEIGHT - 1;
    uint8_t status_color = vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);

    // position hardware cursor at editor cursor location
    int screen_y = state.cursor_y - state.scroll_offset;
    int screen_x = state.cursor_x - state.lines[state.cursor_y].h_scroll;

    if (screen_y >= 0 && screen_y < VGA_HEIGHT - 1 &&
        screen_x >= 0 && screen_x < VGA_WIDTH) {
        vga_update_cursor(screen_x, screen_y);
    }

    // clean status line with inverted color
    for (int x = 0; x < VGA_WIDTH; x++)
        vga[status_row * VGA_WIDTH + x] = vga_entry(' ', status_color);

    // write "-- MODE --"
    const char *mode_text;
    if (in_command_line) {
        mode_text = command_buffer;
        // prefix with ":" manually below instead
    } else {
        mode_text = (state.mode == EDITOR_MODE_INSERT) ? "-- INSERT --" : "-- COMMAND --";
    }

    int pos = 0;
    if (in_command_line) {
        vga[status_row * VGA_WIDTH] = vga_entry(':', status_color);
        pos = 1;
        for (int i = 0; i < command_len; i++) {
            vga[status_row * VGA_WIDTH + pos] = vga_entry(command_buffer[i], status_color);
            pos++;
        }
    } else {
        while (mode_text[pos]) {
            vga[status_row * VGA_WIDTH + pos] = vga_entry(mode_text[pos], status_color);
            pos++;
        }
    }

    // write file name starting on column 20
    int filepath_col = 20;
    pos = 0;
    while (state.filepath[pos] && filepath_col + pos < VGA_WIDTH) {
        vga[status_row * VGA_WIDTH + filepath_col + pos] = vga_entry(state.filepath[pos], status_color);
        pos++;
    }
}

void editor_close(void) {
    t_force_redraw();
    vga_update_cursor(t_column, t_row);
}

static void editor_load_from_file(struct vfs_file *file) {
    uint32_t size = file->fat32.size;
    if (size > sizeof(load_buf))
        size = sizeof(load_buf);

    uint32_t read = vfs_read(file, load_buf, size);

    state.line_count = 0;
    uint32_t i = 0;
    while (i < read && state.line_count < EDITOR_MAX_LINES) {
        struct editor_line *line = &state.lines[state.line_count];
        int len = 0;

        while (i < read && load_buf[i] != '\n' && len < EDITOR_LINE_LENGTH - 1) {
            line->text[len++] = (char)load_buf[i];
            i++;
        }
        while (i < read && load_buf[i] != '\n')
            i++;

        line->length = len;
        line->h_scroll = 0;
        state.line_count++;

        if (i < read && load_buf[i] == '\n')
            i++;
    }

    if (state.line_count == 0) {
        state.line_count = 1;
        state.lines[0].length = 0;
        state.lines[0].h_scroll = 0;
    }
}

static void editor_insert_char(char c) {
    struct editor_line *line = &state.lines[state.cursor_y];

    if (line->length >= EDITOR_LINE_LENGTH - 1)
        return;

    for (int i = line->length; i > state.cursor_x; i--)
        line->text[i] = line->text[i - 1];

    line->text[state.cursor_x] = c;
    line->length++;
    state.cursor_x++;
    state.modified = 1;
}

static void editor_delete_before_cursor(void) {
    struct editor_line *line = &state.lines[state.cursor_y];

    if (state.cursor_x > 0) {
        for (int i = state.cursor_x - 1; i < line->length - 1; i++)
            line->text[i] = line->text[i + 1];

        line->length--;
        state.cursor_x--;
        state.modified = 1;
        return;
    }

    // cursor is at start of line, merge with previous line
    if (state.cursor_y == 0)
        return;

    struct editor_line *prev = &state.lines[state.cursor_y - 1];
    int prev_len = prev->length;

    // append current line text to previous line
    for (int i = 0; i < line->length; i++)
        prev->text[prev_len + i] = line->text[i];
    prev->length += line->length;

    // shift all lines below up by one
    for (int i = state.cursor_y; i < state.line_count - 1; i++)
        state.lines[i] = state.lines[i + 1];

    state.line_count--;
    state.cursor_y--;
    state.cursor_x = prev_len;
    state.modified = 1;
}

static void editor_new_line(void) {
    if (state.line_count >= EDITOR_MAX_LINES)
        return;

    struct editor_line *current = &state.lines[state.cursor_y];
    struct editor_line *next = &state.lines[state.cursor_y + 1];

    for (int i = state.line_count; i > state.cursor_y + 1; i--)
        state.lines[i] = state.lines[i - 1];

    int tail_len = current->length - state.cursor_x;
    for (int i = 0; i < tail_len; i++)
        next->text[i] = current->text[state.cursor_x + i];
    next->length = tail_len;
    next->h_scroll = 0;

    current->length = state.cursor_x;

    state.line_count++;
    state.cursor_y++;
    state.cursor_x = 0;
    state.modified = 1;
}

void editor_input(int key) {
    // arrows work in both modes
    switch (key) {
        case KEY_LEFT:
            if (state.cursor_x > 0) state.cursor_x--;
            editor_render();
            return;
        case KEY_RIGHT:
            if (state.cursor_x < state.lines[state.cursor_y].length) state.cursor_x++;
            editor_render();
            return;
        case KEY_UP:
            if (state.cursor_y > 0) state.cursor_y--;
            editor_render();
            return;
        case KEY_DOWN:
            if (state.cursor_y < state.line_count - 1) state.cursor_y++;
            editor_render();
            return;
    }

    // command line mode, after pressing :
    if (in_command_line) {
        if (key == '\n') {
            command_buffer[command_len] = '\0';
            in_command_line = 0;
            command_len = 0;

            if (k_strcmp(command_buffer, "w") == 0) {
                editor_save();
            } else if (k_strcmp(command_buffer, "q") == 0) {
                editor_close();
                editor_active = 0;
                shell_prompt();
                return;
            } else if (k_strcmp(command_buffer, "wq") == 0) {
                editor_save();
                editor_close();
                editor_active = 0;
                shell_prompt();
                return;
            }
        } else if (key == 27) {
            in_command_line = 0;
            command_len = 0;
        } else if (key == '\b') {
            if (command_len > 0)
                command_len--;
        } else if (command_len < 15) {
            command_buffer[command_len++] = (char)key;
        }

        editor_render();
        return;
    }

    if (state.mode == EDITOR_MODE_COMMAND) {
        if (key == 'i') {
            state.mode = EDITOR_MODE_INSERT;
        } else if (key == ':') {
            in_command_line = 1;
            command_len = 0;
        }
    } else { // insert mode
        if (key == 27) {
            state.mode = EDITOR_MODE_COMMAND;
        } else if (key == '\n') {
            editor_new_line();
        } else if (key == '\b') {
            editor_delete_before_cursor();
        } else if (key >= ' ' && key <= 0xFF) {
            editor_insert_char((char)key);
        }
    }

    editor_render();
}

static void editor_save(void) {
    struct vfs_file file;
    uint8_t buf[EDITOR_LINE_LENGTH + 1];
    uint32_t written;

    // try to open, create if it does not exist
    if (!vfs_open(state.filepath, &file)) {
        if (!vfs_create(state.filepath))
            return;
        if (!vfs_open(state.filepath, &file))
            return;
    }

    // write each line followed by a newline
    for (int i = 0; i < state.line_count; i++) {
        struct editor_line *line = &state.lines[i];
        int len = 0;

        for (int j = 0; j < line->length; j++)
            buf[len++] = line->text[j];

        buf[len++] = '\n';

        written = vfs_write(&file, buf, len);
        if (written != (uint32_t)len)
            return;
    }

    state.modified = 0;
}

int editor_is_active(void) {
    return editor_active;
}