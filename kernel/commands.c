#include "../include/commands.h"
#include "../include/kstring.h"
#include "../include/vga.h"

#define CMD_MAX_ARGS 16

typedef void (*cmd_func_t)(int argc, char **argv);

struct command {
  const char *name;
  const char *description;
  cmd_func_t run;
};

static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_argc(int argc, char **argv);

// Command table.
static const struct command commands[] = {
  {"help", "lists available commands", cmd_help},
  {"clear", "clears the screen", cmd_clear},
  {"echo", "prints the received arguments", cmd_echo},
  {"argc", "shows how many arguments were received", cmd_argc},
};

static const int command_count = sizeof(commands) / sizeof(commands[0]);

static void cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;

  t_print("Available commands:\n");
  for (int i = 0; i < command_count; i++) {
    t_print("  ");
    t_print(commands[i].name);
    t_print(" - ");
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
    t_print(argv[i]);
  }

  t_putchar('\n');
}

static void cmd_argc(int argc, char **argv) {
  char number[12];
  (void)argv;

  k_itoa(argc, number, 10);
  t_print(number);
  t_putchar('\n');
}

void cmd_execute(char *line) {
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

  t_print(argv[0]);
  t_print(": command not found\n");
}