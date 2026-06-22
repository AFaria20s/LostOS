#include "../include/commands.h"
#include "../include/kstring.h"
#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/keyboard_layouts.h"
#include "../include/memory.h"

// Maximum arguments per command
#define CMD_MAX_ARGS 16

typedef void (*cmd_func_t)(int argc, char **argv);

struct command {
  const char *name;
  const char *description;
  cmd_func_t run;
};

// Builtin command implementations
static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_argc(int argc, char **argv);
static void cmd_history(int argc, char **argv);
static void cmd_sudo(int argc, char **argv);
static void cmd_layout(int argc, char **argv);
static void cmd_mem(int argc, char **argv);

// Command table
// leave description empty/NULL to not show on "help"
static const struct command commands[] = {
  {"help", "lists available commands", cmd_help},
  {"clear", "clears the screen", cmd_clear},
  {"echo", "prints the received arguments", cmd_echo},
  {"argc", "shows how many arguments were received", cmd_argc},
  {"history", "prints command history", cmd_history},
  {"sudo", NULL, cmd_sudo},
  {"layout", "show or set keyboard layout", cmd_layout},
  {"mem", "shows kernel memory usage", cmd_mem},
};

static const int command_count = sizeof(commands) / sizeof(commands[0]);

static void print_padded(const char *text, int width) {
  int len = 0;

  t_print_raw(text);
  while (text[len])
    len++;

  while (len++ < width)
    t_putchar(' ');
}

static void cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;

  t_print("$bCommand     Description$f\n\n");
  for (int i = 0; i < command_count; i++) {
    if (commands[i].description == NULL || commands[i].description[0] == '\0')
      continue;

    print_padded(commands[i].name, 12);
    t_print(commands[i].description);
    t_putchar('\n');
  }
}

static void cmd_clear(int argc, char **argv) {
  (void)argc;
  (void)argv;

  t_clear();
}

static void cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (i > 1)
      t_putchar(' ');
    t_print_raw(argv[i]);
  }

  t_putchar('\n');
}

static void cmd_argc(int argc, char **argv) {
  char number[12];
  (void)argv;

  k_itoa(argc, number, 10);
  t_print_raw(number);
  t_putchar('\n');
}

static void cmd_history(int argc, char **argv) {
  (void)argc;
  (void)argv;

  shell_print_history();
}

static void cmd_sudo(int argc, char **argv) {
  (void)argc;
  (void)argv;

  t_print("Nice try! Im just a tea pot...\n");
}

static void cmd_layout(int argc, char **argv) {
  if (argc == 1) {
    const char *cur = keyboard_get_layout();
    t_print("Current layout: ");
    t_print_raw(cur);
    t_print("\nAvailable layouts:");
    for (size_t i = 0; i < layouts_count; i++) {
      t_putchar(' ');
      t_print_raw(layouts[i].name);
    }
    t_putchar('\n');
    return;
  }

  if (keyboard_set_layout(argv[1]) == 0) {
    t_print("Layout set to ");
    t_print_raw(argv[1]);
    t_putchar('\n');
  } else {
    t_print("Unknown layout: ");
    t_print_raw(argv[1]);
    t_putchar('\n');
  }
}

static void print_uint(size_t value) {
  char tmp[32];
  char out[32];
  int i = 0;
  int j = 0;

  do {
    tmp[i++] = (char)('0' + (value % 10));
    value /= 10;
  } while (value && i < (int)sizeof(tmp));

  while (i > 0)
    out[j++] = tmp[--i];
  out[j] = '\0';
  t_print_raw(out);
}

static void print_hex(uintptr_t value) {
  char out[11];
  const char *digits = "0123456789abcdef";

  out[0] = '0';
  out[1] = 'x';
  for (int i = 0; i < 8; i++) {
    int shift = 28 - i * 4;
  out[2 + i] = digits[(value >> shift) & 0xF];
  }
  out[10] = '\0';
  t_print_raw(out);
}

static void print_kib(const char *label, size_t bytes) {
  t_print_raw(label);
  print_uint(bytes / 1024);
  t_print(" KiB\n");
}

static void print_bytes_and_kib(const char *label, size_t bytes) {
  t_print_raw(label);
  print_uint(bytes);
  t_print(" bytes (");
  print_uint(bytes / 1024);
  t_print(" KiB)\n");
}

static void print_test_step(const char *label, int ok) {
  t_print(ok ? "$a[ OK  ]$f " : "$c[ FAIL ]$f ");
  t_print_raw(label);
  t_putchar('\n');
}

static void cmd_mem_test_detail(int simulate_fail) {
  struct memory_stats before;
  struct memory_stats after_alloc;
  struct memory_stats after_free;
  char *ptr = NULL;
  int ok = 1;

  memory_get_stats(&before);
  print_test_step("read memory stats", memory_is_ready());
  if (!memory_is_ready())
    return;

  ptr = (char *)kmalloc(64);
  print_test_step("kmalloc 64 bytes", ptr != NULL);
  if (!ptr)
    return;

  memory_get_stats(&after_alloc);
  print_test_step("allocation counter increased",
                  after_alloc.allocation_count == before.allocation_count + 1);

  for (int i = 0; i < 64; i++)
    ptr[i] = (char)i;
  print_test_step("write test pattern", 1);

  for (int i = 0; i < 64; i++) {
    if (ptr[i] != (char)i)
      ok = 0;
  }
  print_test_step("read test pattern", ok);

  if (simulate_fail) {
    // Temporary failure hook: remove this branch when you no longer need it.
    print_test_step("simulated failure", 0);
  }

  kfree(ptr);
  memory_get_stats(&after_free);
  print_test_step("kfree block", after_free.allocation_count == before.allocation_count);
}

static void cmd_mem(int argc, char **argv) {
  struct memory_stats stats;

  if (argc > 1 && k_strcmp(argv[1], "test") == 0) {
    if (argc > 2 && k_strcmp(argv[2], "-d") == 0) {
      int simulate_fail = argc > 3 && k_strcmp(argv[3], "fail") == 0;
      cmd_mem_test_detail(simulate_fail);
      return;
    }

    char *ptr = (char *)kmalloc(64);

    if (!ptr) {
      t_print("mem test: kmalloc failed\n");
      return;
    }

    for (int i = 0; i < 64; i++)
      ptr[i] = (char)i;

    for (int i = 0; i < 64; i++) {
      if (ptr[i] != (char)i) {
        t_print("mem test: write/read failed\n");
        kfree(ptr);
        return;
      }
    }

    kfree(ptr);
    t_print("mem test: ok\n");
    return;
  }

  memory_get_stats(&stats);

  if (!memory_is_ready()) {
    t_print("$cMemory unavailable$f\n");
    return;
  }

  t_print("$bMemory map$f\n");
  t_print("Kernel      ");
  print_hex(stats.kernel_start);
  t_print(" - ");
  print_hex(stats.kernel_end);
  t_putchar('\n');

  t_print("Heap        ");
  print_hex(stats.heap_start);
  t_print(" - ");
  print_hex(stats.heap_end);
  t_putchar('\n');

  t_print("\n$bUsage$f\n");
  print_kib("Total       ", stats.total_bytes);
  print_bytes_and_kib("Heap size   ", stats.heap_bytes);
  print_bytes_and_kib("Used heap   ", stats.used_bytes);
  print_bytes_and_kib("Free heap   ", stats.free_bytes);
  t_print("Allocations ");
  print_uint(stats.allocation_count);
  t_putchar('\n');
}

void cmd_execute(char *line) {
  // Split the line and execute the matching command
  char *argv[CMD_MAX_ARGS];
  int argc = k_split(line, argv, CMD_MAX_ARGS);

  if (argc == 0)
    return;

  for (int i = 0; i < command_count; i++) {
    if (k_strcmp(argv[0], commands[i].name) == 0) {
      commands[i].run(argc, argv);
      return;
    }
  }

  // comando não encontrado
  t_print_raw(argv[0]);
  t_print(": command not found\n");
}
