#include "../include/commands.h"
#include "../include/keyboard.h"
#include "../include/shell.h"
#include "../include/vga.h"

#define BUFFER_SIZE 256

static char input_buffer[BUFFER_SIZE];
static int buffer_len = 0;
static int cursor_pos = 0;

// Position where text starts after the prompt.
static size_t prompt_col = 0;
static size_t prompt_row = 0;

// Converts a buffer position to a screen position.
static void shell_screen_pos(int pos, size_t *col, size_t *row) {
  size_t total = prompt_row * VGA_WIDTH + prompt_col + (size_t)pos;
  *row = (total / VGA_WIDTH) % VGA_HEIGHT;
  *col = total % VGA_WIDTH;
}

static void shell_move_cursor_to(int pos) {
  size_t col, row;
  shell_screen_pos(pos, &col, &row);
  t_column = col;
  t_row = row;
  vga_update_cursor(col, row);
}

// Redraws the line after inserting or deleting text.
static void shell_redraw_from(int from) {
  size_t col, row;

  for (int i = from; i < buffer_len; i++) {
    shell_screen_pos(i, &col, &row);
    t_putentryat(input_buffer[i], t_color, col, row);
  }

  shell_screen_pos(buffer_len, &col, &row);
  t_putentryat(' ', t_color, col, row);

  shell_move_cursor_to(cursor_pos);
}

static void shell_insert_char(char c) {
  if (buffer_len >= BUFFER_SIZE - 1) return;

  for (int i = buffer_len; i > cursor_pos; i--) {
    input_buffer[i] = input_buffer[i - 1];
  }

  input_buffer[cursor_pos] = c;
  buffer_len++;
  cursor_pos++;

  shell_redraw_from(cursor_pos - 1);
}

static void shell_delete_before_cursor(void) {
  if (cursor_pos == 0) return;

  for (int i = cursor_pos - 1; i < buffer_len - 1; i++) {
    input_buffer[i] = input_buffer[i + 1];
  }

  buffer_len--;
  cursor_pos--;

  shell_redraw_from(cursor_pos);
}

static void shell_delete_at_cursor(void) {
  if (cursor_pos >= buffer_len) return;

  for (int i = cursor_pos; i < buffer_len - 1; i++) {
    input_buffer[i] = input_buffer[i + 1];
  }

  buffer_len--;

  shell_redraw_from(cursor_pos);
}

static void shell_move_left(void) {
  if (cursor_pos > 0) {
    cursor_pos--;
    shell_move_cursor_to(cursor_pos);
  }
}

static void shell_move_right(void) {
  if (cursor_pos < buffer_len) {
    cursor_pos++;
    shell_move_cursor_to(cursor_pos);
  }
}

void shell_prompt(void) {
  t_print("$c404$f@$30x194$f:~$ ");

  prompt_col = t_column;
  prompt_row = t_row;

  shell_move_cursor_to(0);
}

void shell_process(void) {
  input_buffer[buffer_len] = '\0';

  // Move to the end of the line before running the command.
  size_t col, row;
  shell_screen_pos(buffer_len, &col, &row);
  t_column = col;
  t_row = row;

  t_putchar('\n');
  cmd_execute(input_buffer);

  buffer_len = 0;
  cursor_pos = 0;

  shell_prompt();
}

void shell_input(char c) {
  switch (c) {
    case '\n':
      shell_process();
      break;
    case '\b':
      shell_delete_before_cursor();
      break;
    case KEY_DELETE:
      shell_delete_at_cursor();
      break;
    case KEY_LEFT:
      shell_move_left();
      break;
    case KEY_RIGHT:
      shell_move_right();
      break;
    default:
      shell_insert_char(c);
      break;
  }
}
