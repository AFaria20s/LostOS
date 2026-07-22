#include "shell/commands.h"
#include "arch/io.h"
#include "drivers/keyboard.h"
#include "lib/kstring.h"
#include "mm/memory.h"
#include "shell/shell.h"
#include "drivers/vga.h"
#include "shell/sysinfo.h"

#define BUFFER_SIZE 256
#define HISTORY_LIMIT 64
#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

struct history_entry {
  int seq;
  char timestamp[9];
  char *line;
  struct history_entry *older;
  struct history_entry *newer;
};

static char input_buffer[BUFFER_SIZE];
static int buffer_len = 0;
static int cursor_pos = 0;

static struct history_entry *history_oldest = NULL;
static struct history_entry *history_newest = NULL;
static struct history_entry *history_pos = NULL;
static int history_count = 0;
static int history_seq = 0;
static char history_draft[BUFFER_SIZE];
static size_t prompt_col = 0;
static size_t prompt_row = 0;

static char cwd[256] = "/";

const char *shell_get_cwd(void) {
    return cwd;
}

void shell_set_cwd(const char *path) {
  k_strcp(cwd, path);
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
  history_pos = NULL;
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
  uint8_t second, minute, hour, status_b, last_second;
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

static char *history_copy_line(const char *line) {
  size_t len = k_strlen(line);
  char *copy = (char *)kmalloc(len + 1);

  if (!copy)
    return NULL;

  for (size_t i = 0; i <= len; i++)
    copy[i] = line[i];

  return copy;
}

static void history_free_entry(struct history_entry *entry) {
  if (!entry)
    return;

  kfree(entry->line);
  kfree(entry);
}

static void history_drop_oldest(void) {
  struct history_entry *entry = history_oldest;

  if (!entry)
    return;

  history_oldest = entry->newer;
  if (history_oldest)
    history_oldest->older = NULL;
  else
    history_newest = NULL;

  if (history_pos == entry)
    history_pos = NULL;

  history_count--;
  history_free_entry(entry);
}

static void history_add(const char *line) {
  struct history_entry *entry = (struct history_entry *)kmalloc(sizeof(struct history_entry));

  if (!entry)
    return;

  entry->line = history_copy_line(line);
  if (!entry->line) {
    kfree(entry);
    return;
  }

  entry->seq = ++history_seq;
  history_read_timestamp(entry->timestamp);
  entry->older = history_newest;
  entry->newer = NULL;

  if (history_newest)
    history_newest->newer = entry;
  else
    history_oldest = entry;

  history_newest = entry;
  history_count++;
  history_pos = NULL;

  while (history_count > HISTORY_LIMIT)
    history_drop_oldest();
}

static const char *history_prev(void) {
  if (history_count == 0)
    return NULL;

  if (!history_pos) {
    history_save_draft();
    history_pos = history_newest;
    return history_pos->line;
  }

  if (!history_pos->older)
    return history_pos->line;

  history_pos = history_pos->older;
  return history_pos->line;
}

static const char *history_next(void) {
  if (history_count == 0 || !history_pos)
    return NULL;

  if (!history_pos->newer) {
    history_pos = NULL;
    return history_draft;
  }

  history_pos = history_pos->newer;
  return history_pos->line;
}

void shell_print_history(void) {
  char number[12];
  struct history_entry *entry = history_oldest;

  if (history_count == 0) {
    t_print("history: empty\n");
    return;
  }

  t_print("$bNo  Time      Command$f\n\n");

  while (entry) {
    k_itoa(entry->seq, number, 10);
    t_print_raw(number);
    t_print("   ");
    t_print_raw(entry->timestamp);
    t_print("  ");
    t_print_raw(entry->line);
    t_putchar('\n');

    entry = entry->newer;
  }
}

static void shell_screen_pos(int pos, size_t *col, size_t *row);
static void shell_move_cursor_to(int pos);

static void shell_ensure_input_visible(int len) {
  size_t needed_row = prompt_row + (prompt_col + (size_t)len) / VGA_WIDTH;

  while (needed_row >= VGA_HEIGHT) {
    t_scroll();
    if (prompt_row > 0)
      prompt_row--;
    needed_row--;
  }
}

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
  shell_ensure_input_visible(buffer_len);

  size_t col, row;
  for (int j = 0; j < buffer_len; j++) {
    shell_screen_pos(j, &col, &row);
    t_putentryat(input_buffer[j], t_color, col, row);
  }
  for (int j = buffer_len; j < prev_len; j++) {
    shell_screen_pos(j, &col, &row);
    t_putentryat(' ', t_color, col, row);
  }

  shell_move_cursor_to(cursor_pos);
}

static void shell_screen_pos(int pos, size_t *col, size_t *row) {
  size_t total = prompt_row * VGA_WIDTH + prompt_col + (size_t)pos;
  *row = total / VGA_WIDTH;
  *col = total % VGA_WIDTH;
}

static void shell_move_cursor_to(int pos) {
  size_t col, row;
  shell_screen_pos(pos, &col, &row);
  t_column = col;
  t_row = row;
  vga_update_cursor(col, row);
}

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
  if (buffer_len >= BUFFER_SIZE - 1)
    return;
  history_stop_navigation();
  shell_ensure_input_visible(buffer_len + 1);

  for (int i = buffer_len; i > cursor_pos; i--) {
    input_buffer[i] = input_buffer[i - 1];
  }

  input_buffer[cursor_pos] = c;
  buffer_len++;
  cursor_pos++;

  input_buffer[buffer_len] = '\0';

  shell_redraw_from(cursor_pos - 1);

  shell_redraw_from(cursor_pos - 1);
}

static void shell_delete_before_cursor(void) {
  if (cursor_pos == 0)
    return;
  history_stop_navigation();

  for (int i = cursor_pos - 1; i < buffer_len - 1; i++) {
    input_buffer[i] = input_buffer[i + 1];
  }

  buffer_len--;
  cursor_pos--;

  input_buffer[buffer_len] = '\0';

  shell_redraw_from(cursor_pos);

  shell_redraw_from(cursor_pos);
}

static void shell_delete_at_cursor(void) {
  if (cursor_pos >= buffer_len)
    return;
  history_stop_navigation();

  for (int i = cursor_pos; i < buffer_len - 1; i++) {
    input_buffer[i] = input_buffer[i + 1];
  }

  buffer_len--;

  input_buffer[buffer_len] = '\0';

  shell_redraw_from(cursor_pos);

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
  
  t_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  t_print("$c404$f@");
  t_setcolor(vga_entry_color(3, VGA_COLOR_BLACK));
  t_print(OS_NAME);
  t_print("$f:");
  t_print(shell_get_cwd());
  t_print("$ ");

  prompt_col = t_column;
  prompt_row = t_row;

  shell_move_cursor_to(0);
}

void shell_process(void) {
  input_buffer[buffer_len] = '\0';

  size_t col, row;
  shell_ensure_input_visible(buffer_len);
  shell_screen_pos(buffer_len, &col, &row);
  t_column = col;
  t_row = row;

  t_putchar('\n');
  if (buffer_len > 0)
    history_add(input_buffer);
  cmd_execute(input_buffer);

  buffer_len = 0;
  cursor_pos = 0;

  if (!editor_is_active())
    shell_prompt();
}

static void shell_putc(char c) {
    shell_insert_char(c);
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
    case KEY_SCROLL_UP:
      t_scroll_view_up();
      break;
    case KEY_PGDOWN:
    case KEY_SCROLL_DOWN:
      t_scroll_view_down();
      break;
    case KEY_UP:
      {
        const char *h = history_prev();
        if (h)
          shell_replace_input(h);
      }
      break;
    case KEY_DOWN:
      {
        const char *h = history_next();
        if (h)
          shell_replace_input(h);
      }
      break;
    case KEY_TAB: {
      int matches = cmd_autocomplete(input_buffer, shell_putc);
      if (matches > 1) {
          shell_prompt();
          t_print(input_buffer);
      }
      break;
    }
    default:
      if (key >= ' ' && key <= 0xFF)
        shell_insert_char((char)key);
      break;
  }
}
