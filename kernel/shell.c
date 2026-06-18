#include "../include/commands.h"
#include "../include/io.h"
#include "../include/keyboard.h"
#include "../include/kstring.h"
#include "../include/shell.h"
#include "../include/vga.h"

#define BUFFER_SIZE 256
#define HISTORY_SIZE 32
#define CMOS_ADDRESS 0x70 
#define CMOS_DATA    0x71

struct history_entry {
  int seq;
  char timestamp[9];
  char line[BUFFER_SIZE];
};

static char input_buffer[BUFFER_SIZE];
static int buffer_len = 0;
static int cursor_pos = 0;

// Command History
static struct history_entry history[HISTORY_SIZE];
static int history_count = 0; // number of valid entries
static int history_head = 0;  // next write position
static int history_pos = -1;  // current navigation position
static int history_seq = 0;
static char history_draft[BUFFER_SIZE];

static int history_oldest_pos(void) {
  return (history_head - history_count + HISTORY_SIZE) % HISTORY_SIZE;
}

static int history_newest_pos(void) {
  return (history_head - 1 + HISTORY_SIZE) % HISTORY_SIZE;
}

static void history_save_draft(void) {
  int i = 0;
  while (i < BUFFER_SIZE - 1 && i < buffer_len) {
    history_draft[i] = input_buffer[i];
    i++;
  }
  history_draft[i] = '\0';
}

static void history_stop_navigation(void) {
  history_pos = -1;
}

static uint8_t cmos_read(uint8_t reg) {
  outb(CMOS_ADDRESS, reg);
  return inb(CMOS_DATA);
}

static int cmos_updating(void) {
  return (cmos_read(0x0A) & 0x80) != 0;
}

static int bcd_to_binary(uint8_t value) {
  return (value & 0x0F) + ((value / 16) * 10);
}

static void history_format_two_digits(int value, char *out) {
  out[0] = (char)('0' + ((value / 10) % 10));
  out[1] = (char)('0' + (value % 10));
}

static void history_read_timestamp(char out[9]) {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t status_b;
  uint8_t last_second;
  int pm;

  while (cmos_updating()) {}

  second = cmos_read(0x00);
  minute = cmos_read(0x02);
  hour = cmos_read(0x04);
  status_b = cmos_read(0x0B);

  do {
    last_second = second;
    while (cmos_updating()) {}
    second = cmos_read(0x00);
    minute = cmos_read(0x02);
    hour = cmos_read(0x04);
    status_b = cmos_read(0x0B);
  } while (second != last_second);

  pm = (hour & 0x80) != 0;
  hour &= 0x7F;

  if ((status_b & 0x04) == 0) {
    second = (uint8_t)bcd_to_binary(second);
    minute = (uint8_t)bcd_to_binary(minute);
    hour = (uint8_t)bcd_to_binary(hour);
  }

  if ((status_b & 0x02) == 0) {
    if (pm && hour < 12) {
      hour += 12;
    } else if (!pm && hour == 12) {
      hour = 0;
    }
  }

  history_format_two_digits(hour, out);
  out[2] = ':';
  history_format_two_digits(minute, out + 3);
  out[5] = ':';
  history_format_two_digits(second, out + 6);
  out[8] = '\0';
}

// Adds the current line to history (truncates to BUFFER_SIZE-1)
static void history_add(const char *line) {
  int i = 0;
  while (i < BUFFER_SIZE - 1 && line[i]) {
    history[history_head].line[i] = line[i];
    i++;
  }
  history[history_head].line[i] = '\0';
  history[history_head].seq = ++history_seq;
  history_read_timestamp(history[history_head].timestamp);

  history_head = (history_head + 1) % HISTORY_SIZE;
  if (history_count < HISTORY_SIZE) history_count++;
  history_pos = -1;
}

// Navigate to previous entry. Returns pointer to entry or NULL if none.
static const char *history_prev(void) {
  if (history_count == 0) return NULL;

  if (history_pos == -1) {
    history_save_draft();
    history_pos = history_newest_pos();
    return history[history_pos].line;
  }

  if (history_pos == history_oldest_pos()) return history[history_pos].line;

  history_pos = (history_pos - 1 + HISTORY_SIZE) % HISTORY_SIZE;
  return history[history_pos].line;
}

// Navigate to next entry. Returns the saved draft after the newest command.
static const char *history_next(void) {
  if (history_count == 0 || history_pos == -1) return NULL;

  if (history_pos == history_newest_pos()) {
    history_pos = -1;
    return history_draft;
  }

  history_pos = (history_pos + 1) % HISTORY_SIZE;
  return history[history_pos].line;
}

void shell_print_history(void) {
  char number[12];
  int pos = history_oldest_pos();

  if (history_count == 0) {
    t_print("history: empty\n");
    return;
  }

  for (int i = 0; i < history_count; i++) {
    k_itoa(history[pos].seq, number, 10);
    t_print(number);
    t_print("  ");
    t_print(history[pos].timestamp);
    t_print("  ");
    t_print(history[pos].line);
    t_putchar('\n');

    pos = (pos + 1) % HISTORY_SIZE;
  }
}

// Forward declarations used by shell_replace_input
static void shell_screen_pos(int pos, size_t *col, size_t *row);
static void shell_move_cursor_to(int pos);

// Replace the current input buffer with string `s` (handles redraw and clearing)
static void shell_replace_input(const char *s) {
  int prev_len = buffer_len;
  int i = 0;
  while (i < BUFFER_SIZE - 1 && s[i]) {
    input_buffer[i] = s[i];
    i++;
  }
  buffer_len = i;
  input_buffer[buffer_len] = '\0';
  cursor_pos = buffer_len;

  // redraw the whole line region from 0..buffer_len-1
  size_t col, row;
  for (int j = 0; j < buffer_len; j++) {
    shell_screen_pos(j, &col, &row);
    t_putentryat(input_buffer[j], t_color, col, row);
  }
  // clear leftover characters from previous longer line
  for (int j = buffer_len; j < prev_len; j++) {
    shell_screen_pos(j, &col, &row);
    t_putentryat(' ', t_color, col, row);
  }

  shell_move_cursor_to(cursor_pos);
}

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
  history_stop_navigation();

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
  history_stop_navigation();

  for (int i = cursor_pos - 1; i < buffer_len - 1; i++) {
    input_buffer[i] = input_buffer[i + 1];
  }

  buffer_len--;
  cursor_pos--;

  shell_redraw_from(cursor_pos);
}

static void shell_delete_at_cursor(void) {
  if (cursor_pos >= buffer_len) return;
  history_stop_navigation();

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
  if (buffer_len > 0) history_add(input_buffer);
  cmd_execute(input_buffer);

  buffer_len = 0;
  cursor_pos = 0;

  shell_prompt();
}

void shell_input(int key) {
  switch (key) {
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
    case KEY_PGUP:
    case KEY_UP:
      {
        const char *h = history_prev();
        if (h) shell_replace_input(h);
      }
      break;
    case KEY_PGDOWN:
    case KEY_DOWN:
      {
        const char *h = history_next();
        if (h) shell_replace_input(h);
      }
      break;
    default:
      if (key >= ' ' && key <= 0xFF) shell_insert_char((char)key);
      break;
  }
}
