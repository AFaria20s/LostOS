#include "shell/commands.h"
#include "lib/kstring.h"
#include "shell/shell.h"
#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "drivers/keyboard_layouts.h"
#include "mm/memory.h"
#include "mm/paging.h"
#include "shell/sysinfo.h"
#include "drivers/ata.h"
#include "fs/fat32.h"
#include "fs/mbr.h"
#include "fs/vfs.h"
#include "fs/config.h"
#include "editor/editor.h"
#include "lib/path.h"

// Maximum arguments per command
#define CMD_MAX_ARGS 16
#define COMMAND_LINE_MAX 512
#define ECHO_SPACE_MARKER '\x1D'
#define ECHO_TAB_MARKER '\x1E'
#define SHELL_IO_BUFFER_SIZE 4096

typedef void (*command_func_t)(int argc, char **argv);

struct command {
  const char *name;
  const char *description;
  command_func_t run;
};

static void print_hex(uintptr_t value);

// Builtin command implementations
static void print_uint(size_t value);
static void cmd_help(int argc, char **argv);
static void cmd_clear(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_argc(int argc, char **argv);
static void cmd_history(int argc, char **argv);
static void cmd_sudo(int argc, char **argv);
static void cmd_layout(int argc, char **argv);
static void cmd_mem(int argc, char **argv);
static void cmd_paging(int argc, char **argv);
static void cmd_whatami(int argc, char **argv);
static void cmd_atatest(int argc, char **argv);
static void cmd_ls(int argc, char **argv);
static void cmd_read(int argc, char **argv);
static void cmd_touch(int argc, char **argv);
static void cmd_mkdir(int argc, char **argv);
static void cmd_rm(int argc, char **argv);
static void cmd_cp(int argc, char **argv);
static void cmd_mv(int argc, char **argv);
static void cmd_rmdir(int argc, char **argv);
static void cmd_lost(int argc, char **argv);
static void cmd_pwd(int argc, char **argv);
static void cmd_cd(int argc, char **argv);
static void cmd_tree(int argc, char **argv);
static void cmd_cat(int argc, char **argv);
static void cmd_grep(int argc, char **argv);
static void cmd_head(int argc, char **argv);
static void cmd_wc(int argc, char **argv);
static int expand_echo_substitutions(const char *line, char *expanded, size_t capacity);
static int format_echo_line(const char *line, char *formatted, size_t capacity);
static void restore_echo_spaces(char *text);
static int expand_wildcards(char **argv, int argc, char **expanded_argv,
                            int max_args, char *storage, size_t storage_size);

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
  {"paging", "shows paging status", cmd_paging},
  {"whatami", "what are you exactly?", cmd_whatami},
  {"atatest", "test ATA, MBR and FAT32", cmd_atatest},
  {"ls", "list the files in the directory", cmd_ls},
  {"read", "display the content of the file", cmd_read},
  {"touch", "create an empty file", cmd_touch},
  {"mkdir", "create a directory", cmd_mkdir},
  {"rm", "remove a file", cmd_rm},
  {"cp", "copy a file", cmd_cp},
  {"mv", "move or rename a file", cmd_mv},
  {"rmdir", "remove an empty directory", cmd_rmdir},
  {"lost", "open Lost text editor", cmd_lost},
  {"pwd", "print current working direcory", cmd_pwd},
  {"cd", "change directory", cmd_cd},
  {"tree", "check the path structure", cmd_tree},
  {"cat", "prints file or pipe input", cmd_cat},
  {"grep", "filters pipe input by text", cmd_grep},
  {"head", "prints the first lines of pipe input", cmd_head},
  {"wc", "counts lines and bytes from pipe input", cmd_wc},
};

static const int command_count = sizeof(commands) / sizeof(commands[0]);

static const char *shell_input_data;
static size_t shell_input_length;
static int shell_collecting_input;
static char shell_collecting_path[256];

static int shell_begin_input_redirect(const char *path, int append) {
  char resolved[256];
  struct vfs_file file;

  resolve_path(shell_get_cwd(), path, resolved);
  if (!append && vfs_open(resolved, &file) && !vfs_remove(resolved))
    return 0;
  if (!vfs_open(resolved, &file) &&
      (!vfs_create(resolved) || !vfs_open(resolved, &file)))
    return 0;

  k_strcp(shell_collecting_path, resolved);
  shell_collecting_input = 1;
  return 1;
}

static int shell_write_input_line(const char *line) {
  struct vfs_file file;
  char data[COMMAND_LINE_MAX + 1];
  size_t length = k_strlen(line);

  if (length >= COMMAND_LINE_MAX)
    length = COMMAND_LINE_MAX - 1;
  for (size_t i = 0; i < length; i++)
    data[i] = line[i];
  data[length++] = '\n';

  if (!vfs_open(shell_collecting_path, &file))
    return 0;
  file.fat32.offset = file.fat32.size;
  return vfs_write(&file, data, length) == length;
}

int commands_input_active(void) {
  return shell_collecting_input;
}

int commands_input_line(const char *line) {
  if (!shell_collecting_input)
    return 0;

  if (!shell_write_input_line(line))
    t_print("input: write failed\n");
  return 1;
}

void commands_input_eof(void) {
  if (!shell_collecting_input)
    return;

  shell_collecting_input = 0;
  shell_collecting_path[0] = '\0';
  t_print("^D\n");
}

static void tree_print(const char *path, int depth, uint8_t *has_more_siblings) {
    struct vfs_dirent entries[64];
    int count = 0;

    // read all entries first to know how many there are
    while (count < 64 && vfs_readdir(path, count, &entries[count]))
        count++;

    for (int i = 0; i < count; i++) {
        int is_last = (i == count - 1);

        // draw the prefix lines for previous levels
        for (int d = 0; d < depth; d++)
            t_putchar(has_more_siblings[d] ? (char)0xB3 : ' ');

        t_putchar(is_last ? (char)0xC0 : (char)0xC3);
        t_putchar((char)0xC4);
        t_putchar((char)0xC4);
        t_putchar(' ');

        if (entries[i].attributes & 0x10) {
            t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
            t_print_raw(entries[i].name);
            t_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        } else {
            t_print_raw(entries[i].name);
        }
        t_putchar('\n');

        // if it is a directory, enter recursively
        if (entries[i].attributes & 0x10) {
            char child_path[256];
            k_strcp(child_path, path);
            if (path[k_strlen(path) - 1] != '/')
                k_strapp(child_path, "/");
            k_strapp(child_path, entries[i].name);

            has_more_siblings[depth] = !is_last;
            tree_print(child_path, depth + 1, has_more_siblings);
        }
    }
}

static void cmd_tree(int argc, char **argv) {
    char resolved[256];
    uint8_t has_more_siblings[32] = {0};
    const char *path = argc > 1 ? argv[1] : shell_get_cwd();

    resolve_path(shell_get_cwd(), path, resolved);
    t_print_raw(resolved);
    t_putchar('\n');
    tree_print(resolved, 0, has_more_siblings);
}

static void cmd_pwd(int argc, char **argv) {
  t_print(shell_get_cwd());
  t_print("\n");
}

static void cmd_cd(int argc, char **argv) {
    char resolved[256];
    struct vfs_file file;

    if (argc < 2) {
        shell_set_cwd("/");
        return;
    }

    resolve_path(shell_get_cwd(), argv[1], resolved);

    if (!vfs_open(resolved, &file)) {
      t_print_raw(argv[1]);
      t_print(": not found\n");
      return;
    }

    if (!vfs_is_directory(resolved)) {
      t_print_raw(argv[1]);
      t_print(": not a directory\n");
      return;
    }

    shell_set_cwd(resolved);
}

static void cmd_lost(int argc, char **argv) {
  char resolved[256];

  if (argc < 2) {
    t_print("usage: lost <path>\n");
    return;
  }

  resolve_path(shell_get_cwd(), argv[1], resolved);
  editor_open(resolved);
}

static void cmd_read(int argc, char **argv) {
  struct vfs_file file;
  uint8_t buf[512];
  uint32_t bytes_read;
  char resolved[256];

  if (argc < 2) {
    t_print("usage: read <path>\n");
    return;
  }

  resolve_path(shell_get_cwd(), argv[1], resolved);

  if (!vfs_open(resolved, &file)) {
    t_print_raw(argv[1]);
    t_print(": not found\n");
    return;
  }

  while ((bytes_read = vfs_read(&file, buf, sizeof(buf))) > 0) {
    for (uint32_t i = 0; i < bytes_read; i++)
      t_putchar((char)buf[i]);
  }
}

static void cmd_touch(int argc, char **argv) {
  char resolved[256];

  if (argc < 2) {
    t_print("usage: touch <path>\n");
    return;
  }

  resolve_path(shell_get_cwd(), argv[1], resolved);

  if (!vfs_create(resolved)) {
    t_print_raw(argv[1]);
    t_print(": could not create\n");
  }
}

static void cmd_mkdir(int argc, char **argv) {
  char resolved[256];

  if (argc < 2) {
    t_print("usage: mkdir <path>\n");
    return;
  }

  resolve_path(shell_get_cwd(), argv[1], resolved);

  if (!vfs_mkdir(resolved)) {
    t_print_raw(argv[1]);
    t_print(": could not create\n");
  }
}

static void cmd_rm(int argc, char **argv) {
  char resolved[256];

  if (argc < 2) {
    t_print("usage: rm <path>\n");
    return;
  }

  resolve_path(shell_get_cwd(), argv[1], resolved);

  if (!vfs_remove(resolved)) {
    t_print_raw(argv[1]);
    t_print(": could not remove\n");
  }
}

static void cmd_rmdir(int argc, char **argv) {
  char resolved[256];

  if (argc < 2) {
    t_print("usage: rmdir <path>\n");
    return;
  }

  resolve_path(shell_get_cwd(), argv[1], resolved);

  if (!vfs_rmdir(resolved)) {
    t_print_raw(argv[1]);
    t_print(": could not remove\n");
  }
}

static void cmd_mv(int argc, char **argv) {
  char source[256];
  char destination[256];

  if (argc < 3) {
    t_print("usage: mv <src> <dst>\n");
    return;
  }

  resolve_path(shell_get_cwd(), argv[1], source);
  resolve_path(shell_get_cwd(), argv[2], destination);

  if (!vfs_rename(source, destination)) {
    t_print_raw(argv[1]);
    t_print(": could not move\n");
  }
}

static void cmd_cp(int argc, char **argv) {
  struct vfs_file src;
  uint8_t buf[512];
  uint32_t bytes_read;
  char source[256];
  char destination[256];

  if (argc < 3) {
    t_print("usage: cp <src> <dst>\n");
    return;
  }

  resolve_path(shell_get_cwd(), argv[1], source);
  resolve_path(shell_get_cwd(), argv[2], destination);

  if (!vfs_open(source, &src)) {
    t_print_raw(argv[1]);
    t_print(": not found\n");
    return;
  }

  if (!vfs_create(destination)) {
    t_print_raw(argv[2]);
    t_print(": could not create\n");
    return;
  }

  {
    struct vfs_file dst;

    if (!vfs_open(destination, &dst)) {
      t_print_raw(argv[2]);
      t_print(": could not open\n");
      return;
    }

    while ((bytes_read = vfs_read(&src, buf, sizeof(buf))) > 0) {
      if (vfs_write(&dst, buf, bytes_read) != bytes_read) {
        t_print("cp: write failed\n");
        return;
      }
    }
  }
}

static void cmd_ls(int argc, char **argv) {
    struct vfs_dirent entry;
    char path[256];
    int index = 0;

    resolve_path(shell_get_cwd(), argc > 1 ? argv[1] : ".", path);

    while (vfs_readdir(path, index++, &entry)) {
      if (entry.attributes & 0x10) // if it is folder
        t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
      else
        t_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
      
      t_print_raw(entry.name);
      t_putchar('\n');
    }

    t_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

static void cmd_atatest(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!ata_init()) {
    t_print("ATA initialization failed\n");
    return;
  }

  t_print("ATA ready\n");

  if (!mbr_init()) {
    t_print("MBR read or validation failed\n");
    return;
  }

  t_print("MBR signature valid\n");

  for (int i = 0; i < MBR_PARTITION_COUNT; i++) {
    const struct mbr_partition *partition = mbr_get_partition(i);

    t_print("Partition ");
    print_uint(i);
    t_print(": status ");
    print_hex(partition->status);
    t_print(", type ");
    print_hex(partition->type);
    t_print(", lba ");
    print_uint(partition->lba_start);
    t_print(", sectors ");
    print_uint(partition->sector_count);
    t_putchar('\n');
  }

  if (!fat32_init()) {
    t_print("FAT32 initialization failed\n");
    return;
  }

  t_print("FAT32 ready\n");

  for (int i = 0; ; i++) {
    struct fat32_dirent entry;

    if (!fat32_readdir("/", i, &entry))
      break;

    t_print("File ");
    t_print(entry.name);
    t_print(", size ");
    print_uint(entry.size);
    t_putchar('\n');
  }

  {
    struct fat32_dirent entry;

    if (!fat32_readdir("DOCS", 0, &entry)) {
      t_print("FAT32 directory read failed\n");
      return;
    }

    t_print("DOCS/");
    t_print(entry.name);
    t_putchar('\n');
  }

  {
    struct fat32_file file;
    char buffer[128];
    uint32_t size;

    if (!fat32_open("HELLO.TXT", &file)) {
      t_print("FAT32 file open failed\n");
      return;
    }

    size = fat32_read(&file, buffer, sizeof(buffer) - 1);
    buffer[size] = '\0';
    t_print("HELLO.TXT: ");
    t_print(buffer);
    t_putchar('\n');

    if (!fat32_open("DOCS/INFO.TXT", &file)) {
      t_print("FAT32 nested file open failed\n");
      return;
    }

    size = fat32_read(&file, buffer, sizeof(buffer) - 1);
    buffer[size] = '\0';
    t_print("DOCS/INFO.TXT: ");
    t_print(buffer);
    t_putchar('\n');
  }
}

static int autocomplete_command(const char *prefix, void (*putc)(char))
{
    int prefix_len = k_strlen(prefix);
    const char *match = NULL;
    int matches = 0;

    for (int i = 0; i < command_count; i++) {
        if (k_strncmp(commands[i].name, prefix, prefix_len) == 0) {
            match = commands[i].name;
            matches++;
        }
    }

    if (matches == 0)
        return 0;

    if (matches == 1) {
        const char *p = match + prefix_len;

        while (*p)
            putc(*p++);

        return 1;
    }

    t_putchar('\n');

    for (int i = 0; i < command_count; i++) {
        if (k_strncmp(commands[i].name, prefix, prefix_len) == 0) {
            t_print(commands[i].name);
            t_putchar(' ');
        }
    }

    t_putchar('\n');

    return matches;
}

static int name_has_prefix(const char *name, const char *prefix) {
    while (*prefix) {
        char name_character = *name;
        char prefix_character = *prefix;

        if (name_character >= 'A' && name_character <= 'Z')
            name_character += 'a' - 'A';
        if (prefix_character >= 'A' && prefix_character <= 'Z')
            prefix_character += 'a' - 'A';

        if (name_character != prefix_character)
            return 0;

        name++;
        prefix++;
    }

    return 1;
}

static int autocomplete_path(const char *input, void (*putc)(char)) {
    char directory_input[256];
    char directory[256];
    char common[13];
    const char *token;
    const char *name_prefix;
    const char *slash = NULL;
    struct vfs_dirent entry;
    int input_length = k_strlen(input);
    int token_start = input_length;
    int matches = 0;
    int only_match_is_directory = 0;
    int index = 0;

    while (token_start > 0 && input[token_start - 1] != ' ' && input[token_start - 1] != '\t')
        token_start--;

    token = input + token_start;
    for (const char *character = token; *character; character++) {
        if (*character == '/')
            slash = character;
    }

    if (slash) {
        int length = slash - token + 1;

        for (int i = 0; i < length; i++)
            directory_input[i] = token[i];
        directory_input[length] = '\0';
        name_prefix = slash + 1;
    } else {
        directory_input[0] = '.';
        directory_input[1] = '\0';
        name_prefix = token;
    }

    resolve_path(shell_get_cwd(), directory_input, directory);

    while (vfs_readdir(directory, index++, &entry)) {
        if (!name_has_prefix(entry.name, name_prefix))
            continue;

        if (matches == 0) {
            k_strcp(common, entry.name);
            only_match_is_directory = (entry.attributes & 0x10) != 0;
        } else {
            int length = 0;

            while (common[length] && entry.name[length] &&
                   common[length] == entry.name[length])
                length++;
            common[length] = '\0';
        }

        matches++;
    }

    if (matches == 0)
        return 0;

    for (int i = k_strlen(name_prefix); common[i]; i++)
        putc(common[i]);

    if (matches == 1 && only_match_is_directory)
        putc('/');

    if (matches == 1)
        return 1;

    t_putchar('\n');
    index = 0;
    while (vfs_readdir(directory, index++, &entry)) {
        if (!name_has_prefix(entry.name, name_prefix))
            continue;

        if (entry.attributes & 0x10)
            t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
        else
            t_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

        t_print_raw(entry.name);
        if (entry.attributes & 0x10)
            t_putchar('/');
        t_putchar(' ');
    }
    t_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    t_putchar('\n');

    return matches;
}

int autocomplete_input(const char *input, void (*putc)(char)) {
    int length = k_strlen(input);

    for (int i = 0; i < length; i++) {
        if (input[i] == ' ' || input[i] == '\t')
            return autocomplete_path(input, putc);
    }

    return autocomplete_command(input, putc);
}

static void print_uint2(uint8_t value) {
  if (value < 10)
    t_putchar('0');
  print_uint(value);
}

static void cmd_whatami(int argc, char **argv) {
  cmd_clear(argc, argv);
  const int PADDING = 15;
  struct system_info sysinfo;
  (void)argc;
  (void)argv;

  t_print("$dEnvironment Specifications:\n\n");

  system_getinfo(&sysinfo);
  t_print_padded("OS", PADDING);
  t_print(sysinfo.os_name);
  t_putchar('\n');
  t_print_padded("Architecture", PADDING);
  t_print(sysinfo.architecture);
  t_putchar('\n');
  t_print_padded("Build", PADDING);
  t_print(sysinfo.build);
  t_putchar('\n');
  t_print_padded("CPU", PADDING); t_print(sysinfo.cpu); t_putchar('\n');
  t_print_padded("GPU", PADDING); t_print(sysinfo.gpu); t_putchar('\n');  
  t_print_padded("RAM Size", PADDING);
  print_uint(sysinfo.ram_kib);
  t_print(" KiB\n");
  t_print_padded("Heap Used", PADDING);
  print_uint(sysinfo.heap_used);
  t_print(" bytes\n");
  t_print_padded("Heap Free", PADDING);
  print_uint(sysinfo.heap_free);
  t_print(" bytes\n");
  t_print_padded("Disk Size", PADDING);
  print_uint(sysinfo.disk_mb);
  t_print(" MiB\n");
  t_print_padded("Date", PADDING);
  print_uint2(sysinfo.day);
  t_putchar('/');
  print_uint2(sysinfo.month);
  t_print("/20");
  print_uint2(sysinfo.year);
  t_putchar('\n');
  t_print_padded("Time", PADDING);
  print_uint2(sysinfo.hour);
  t_putchar(':');
  print_uint2(sysinfo.minute);
  t_putchar(':');
  print_uint2(sysinfo.second);
  t_putchar('\n');
}

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

  t_print("$dCommand / Description$f\n\n");
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
    t_print(argv[i]);
  }

  t_putchar('\n');
}

static void print_input(void) {
  if (shell_input_data && shell_input_length > 0)
    t_write(shell_input_data, shell_input_length);
}

static void cmd_cat(int argc, char **argv) {
  char buffer[512];
  struct vfs_file file;
  uint32_t bytes_read;

  if (argc == 1) {
    print_input();
    return;
  }

  for (int i = 1; i < argc; i++) {
    char path[256];
    resolve_path(shell_get_cwd(), argv[i], path);
    if (!vfs_open(path, &file)) {
      t_print_raw(argv[i]);
      t_print(": not found\n");
      continue;
    }
    while ((bytes_read = vfs_read(&file, buffer, sizeof(buffer))) > 0)
      t_write(buffer, bytes_read);
  }
}

static void cmd_grep(int argc, char **argv) {
  const char *needle;
  const char *configured_color;
  char color_code;
  size_t line_start = 0;

  if (argc < 2) {
    t_print("usage: grep <text>\n");
    return;
  }
  needle = argv[1];
  config_reload();
  configured_color = config_get("grep_color");
  if (!configured_color)
    configured_color = config_get("grep.match_color");
  color_code = 'e';
  if (configured_color && configured_color[0]) {
    color_code = configured_color[0] == '$' && configured_color[1]
                   ? configured_color[1] : configured_color[0];
  }

  for (size_t i = 0; i <= shell_input_length; i++) {
    if (i == shell_input_length || shell_input_data[i] == '\n') {
      size_t line_length = i - line_start;
      char line[COMMAND_LINE_MAX];
      size_t needle_length = k_strlen(needle);
      int found = 0;

      if (line_length >= sizeof(line))
        line_length = sizeof(line) - 1;
      for (size_t j = 0; j < line_length; j++)
        line[j] = shell_input_data[line_start + j];
      line[line_length] = '\0';

      if (needle_length == 0)
        found = 1;
      for (size_t j = 0; !found && j + needle_length <= line_length; j++)
        if (k_strncmp(line + j, needle, needle_length) == 0)
          found = 1;

      if (found) {
        if (needle_length == 0) {
          t_print_raw(line);
          if (i < shell_input_length)
            t_putchar('\n');
          line_start = i + 1;
          continue;
        }
        size_t offset = 0;
        while (offset < line_length) {
          size_t match = offset;
          while (match + needle_length <= line_length &&
                 k_strncmp(line + match, needle, needle_length) != 0)
            match++;

          if (match > offset)
            t_write(line + offset, match - offset);
          if (match + needle_length <= line_length) {
            t_set_color_code(color_code);
            t_write(line + match, needle_length);
            t_set_color_code('f');
            offset = match + needle_length;
          } else {
            break;
          }
        }
        if (i < shell_input_length)
          t_putchar('\n');
      }
      line_start = i + 1;
    }
  }
}

static void cmd_head(int argc, char **argv) {
  int wanted = 10;
  int lines = 0;

  if (argc > 1)
    wanted = 0;
  if (argc > 1) {
    for (size_t i = 0; argv[1][i] >= '0' && argv[1][i] <= '9'; i++)
      wanted = wanted * 10 + (argv[1][i] - '0');
  }

  for (size_t i = 0; i < shell_input_length && lines < wanted; i++) {
    t_putchar(shell_input_data[i]);
    if (shell_input_data[i] == '\n')
      lines++;
  }
}

static void cmd_wc(int argc, char **argv) {
  size_t lines = 0;
  (void)argc;
  (void)argv;

  for (size_t i = 0; i < shell_input_length; i++)
    if (shell_input_data[i] == '\n')
      lines++;

  print_uint(lines);
  t_putchar(' ');
  print_uint(shell_input_length);
  t_putchar('\n');
}

static int command_name_is_echo(const char *line) {
  while (*line == ' ' || *line == '\t')
    line++;

  return k_strncmp(line, "echo", 4) == 0 &&
         (line[4] == '\0' || line[4] == ' ' || line[4] == '\t');
}

static int append_text(char *destination, size_t *length, size_t capacity,
                       const char *source, size_t source_length) {
  if (*length + source_length >= capacity)
    return 0;

  for (size_t i = 0; i < source_length; i++)
    destination[(*length)++] = source[i];
  destination[*length] = '\0';
  return 1;
}

static int wildcard_match(const char *pattern, const char *text) {
  if (*pattern == '\0')
    return *text == '\0';
  if (*pattern == '*')
    return wildcard_match(pattern + 1, text) ||
           (*text && wildcard_match(pattern, text + 1));
  return *text && (*pattern == '?' || *pattern == *text) &&
         wildcard_match(pattern + 1, text + 1);
}

static int has_wildcard(const char *text) {
  for (size_t i = 0; text[i]; i++)
    if (text[i] == '*' || text[i] == '?')
      return 1;
  return 0;
}

static int expand_wildcards(char **argv, int argc, char **expanded_argv,
                            int max_args, char *storage, size_t storage_size) {
  int expanded_argc = 0;
  size_t storage_length = 0;

  for (int i = 0; i < argc; i++) {
    const char *argument = argv[i];
    if (!has_wildcard(argument) || i == 0) {
      if (expanded_argc >= max_args)
        break;
      expanded_argv[expanded_argc++] = argv[i];
      continue;
    }

    size_t argument_length = k_strlen(argument);
    size_t slash = argument_length;
    while (slash > 0 && argument[slash - 1] != '/')
      slash--;

    char directory[256];
    char pattern[256];
    if (slash >= sizeof(directory) || argument_length - slash >= sizeof(pattern))
      return 0;

    if (slash == 0)
      k_strcp(directory, shell_get_cwd());
    else {
      for (size_t j = 0; j < slash; j++)
        directory[j] = argument[j];
      directory[slash] = '\0';
    }
    for (size_t j = 0; j < argument_length - slash; j++)
      pattern[j] = argument[slash + j];
    pattern[argument_length - slash] = '\0';

    char resolved_directory[256];
    resolve_path(shell_get_cwd(), directory, resolved_directory);
    int matches = 0;
    for (int index = 0; ; index++) {
      struct vfs_dirent entry;
      if (!vfs_readdir(resolved_directory, index, &entry))
        break;
      if (!wildcard_match(pattern, entry.name))
        continue;
      if (expanded_argc >= max_args ||
          storage_length + k_strlen(resolved_directory) + k_strlen(entry.name) + 2 >=
            storage_size)
        return 0;

      expanded_argv[expanded_argc++] = storage + storage_length;
      if (slash == 0) {
        k_strcp(storage + storage_length, entry.name);
        storage_length += k_strlen(entry.name) + 1;
      } else {
        for (size_t j = 0; j < slash; j++)
          storage[storage_length++] = argument[j];
        k_strcp(storage + storage_length, entry.name);
        storage_length += k_strlen(entry.name) + 1;
      }
      matches++;
    }

    if (matches == 0) {
      if (expanded_argc >= max_args)
        return 0;
      expanded_argv[expanded_argc++] = argv[i];
    }
  }

  expanded_argv[expanded_argc] = NULL;
  return expanded_argc;
}

static int expand_echo_substitutions(const char *line, char *expanded, size_t capacity) {
  size_t output_length = 0;
  size_t input_length = k_strlen(line);
  size_t i = 0;

  while (i < input_length) {
    if (line[i] != '$' || i + 1 >= input_length || line[i + 1] != '(') {
      if (!append_text(expanded, &output_length, capacity, line + i, 1))
        return 0;
      i++;
      continue;
    }

    size_t command_start = i + 2;
    size_t command_end = command_start;
    int depth = 1;

    while (command_end < input_length && depth > 0) {
      if (line[command_end] == '$' && command_end + 1 < input_length &&
          line[command_end + 1] == '(') {
        depth++;
        command_end++;
      } else if (line[command_end] == ')') {
        depth--;
      }
      command_end++;
    }

    if (depth != 0) {
      if (!append_text(expanded, &output_length, capacity, line + i, 1))
        return 0;
      i++;
      continue;
    }

    char command[COMMAND_LINE_MAX];
    size_t command_length = (command_end - 1) - command_start;
    if (command_length >= sizeof(command))
      return 0;

    for (size_t j = 0; j < command_length; j++)
      command[j] = line[command_start + j];
    command[command_length] = '\0';

    char result[COMMAND_LINE_MAX];
    t_capture_begin(result, sizeof(result));
    commands_execute(command);
    size_t result_length = t_capture_end();

    while (result_length > 0 &&
           (result[result_length - 1] == '\n' || result[result_length - 1] == '\r'))
      result[--result_length] = '\0';

    if (!append_text(expanded, &output_length, capacity, result, result_length))
      return 0;
    i = command_end;
  }

  return 1;
}

static char echo_escape_value(char escaped) {
  switch (escaped) {
  case 'n': return '\n';
  case 'r': return '\r';
  case 's': return ' ';
  case 't': return '\t';
  case '\\': return '\\';
  case '"': return '"';
  default: return '\0';
  }
}

static int format_echo_line(const char *line, char *formatted, size_t capacity) {
  size_t length = 0;
  int in_quotes = 0;

  for (size_t i = 0; line[i]; i++) {
    char character = line[i];

    if (character == '"') {
      in_quotes = !in_quotes;
      continue;
    }

    if (in_quotes && character == '\\' && line[i + 1]) {
      char escaped = echo_escape_value(line[++i]);
      if (escaped != '\0')
        character = escaped;
      else {
        if (!append_text(formatted, &length, capacity, "\\", 1) ||
            !append_text(formatted, &length, capacity, line + i, 1))
          return 0;
        continue;
      }
    }

    if (in_quotes && character == ' ')
      character = ECHO_SPACE_MARKER;
    else if (in_quotes && character == '\t')
      character = ECHO_TAB_MARKER;

    if (!append_text(formatted, &length, capacity, &character, 1))
      return 0;
  }

  return 1;
}

static void restore_echo_spaces(char *text) {
  for (size_t i = 0; text[i]; i++) {
    if (text[i] == ECHO_SPACE_MARKER)
      text[i] = ' ';
    else if (text[i] == ECHO_TAB_MARKER)
      text[i] = '\t';
  }
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

static void memory_test_detail(int simulate_fail) {
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

  kfree(ptr);
  memory_get_stats(&after_free);
  print_test_step("kfree block", after_free.allocation_count == before.allocation_count);
}

static void cmd_mem(int argc, char **argv) {
  struct memory_stats stats;

  if (argc > 1 && k_strcmp(argv[1], "test") == 0) {
    if (argc > 2 && k_strcmp(argv[2], "-d") == 0) {
      int simulate_fail = argc > 3 && k_strcmp(argv[3], "fail") == 0;
      memory_test_detail(simulate_fail);
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

  t_print("$dMemory map$f\n");
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

  t_print("\n$dUsage$f\n");
  print_kib("Total       ", stats.total_bytes);
  print_bytes_and_kib("Heap size   ", stats.heap_bytes);
  print_bytes_and_kib("Used heap   ", stats.used_bytes);
  print_bytes_and_kib("Free heap   ", stats.free_bytes);
  t_print("Allocations ");
  print_uint(stats.allocation_count);
  t_putchar('\n');
}

static void cmd_paging(int argc, char **argv) {
  struct paging_stats stats;

  if (argc > 1 && k_strcmp(argv[1], "test") == 0) {
    t_print("paging test: ");
    t_print(paging_test() ? "$aok$f\n" : "$cfailed$f\n");
    return;
  }

  (void)argv;
  paging_get_stats(&stats);

  t_print("$dPaging$f\n");
  t_print("Status      ");
  t_print(stats.enabled ? "$aenabled$f\n" : "$cdisabled$f\n");

  t_print("Directory   ");
  print_hex(stats.directory_addr);
  t_putchar('\n');

  t_print("Mapped      ");
  print_hex(stats.mapped_start);
  t_print(" - ");
  print_hex(stats.mapped_end);
  t_putchar('\n');

  print_kib("Mapped size ", stats.mapped_bytes);
  t_print("Pages       ");
  print_uint(stats.page_count);
  t_putchar('\n');
}

static void script_execute(const char *path) {
    struct vfs_file file;
    char buf[512];
    uint32_t bytes_read;
    char line_buf[256];
    int line_len = 0;

    if (!vfs_open(path, &file))
        return;

    while ((bytes_read = vfs_read(&file, buf, sizeof(buf))) > 0) {
        for (uint32_t i = 0; i < bytes_read; i++) {
            if (buf[i] == '\n') {
                line_buf[line_len] = '\0';
                if (line_len > 0)
                    commands_execute(line_buf);
                line_len = 0;
            } else if (line_len < 255) {
                line_buf[line_len++] = buf[i];
            }
        }
    }

    if (line_len > 0) {
        line_buf[line_len] = '\0';
        commands_execute(line_buf);
    }
}

static int commands_execute_simple(char *line) {
  char *argv[CMD_MAX_ARGS];
  char *expanded_argv[CMD_MAX_ARGS];
  char expanded_line[COMMAND_LINE_MAX];
  char formatted_line[COMMAND_LINE_MAX];
  char wildcard_storage[COMMAND_LINE_MAX];
  int argc;
  int expanded_argc;

  if (command_name_is_echo(line)) {
    if (!expand_echo_substitutions(line, expanded_line, sizeof(expanded_line)))
      return 1;
    if (!format_echo_line(expanded_line, formatted_line, sizeof(formatted_line)))
      return 1;
    line = formatted_line;
  }

  argc = k_split(line, argv, CMD_MAX_ARGS);

  if (argc == 0)
    return 0;

  if (command_name_is_echo(line)) {
    for (int i = 1; i < argc; i++)
      restore_echo_spaces(argv[i]);
  }

  expanded_argc = expand_wildcards(argv, argc, expanded_argv, CMD_MAX_ARGS - 1,
                                   wildcard_storage, sizeof(wildcard_storage));
  if (expanded_argc <= 0)
    return 1;
  argc = expanded_argc;
  for (int i = 0; i < argc; i++)
    argv[i] = expanded_argv[i];

  for (int i = 0; i < command_count; i++) {
    if (k_strcmp(argv[0], commands[i].name) == 0) {
      commands[i].run(argc, argv);
      return 0;
    }
  }

  // if command not found in default commands, search in /bin directory for custom commands
  char script_path[256];
  struct vfs_file test;

  k_strcat(script_path, "/bin/", argv[0], "");
  k_strapp(script_path, ".lts");

  if (vfs_open(script_path, &test)) {
      script_execute(script_path);
      return 0;
  }

  t_print_raw(argv[0]);
  t_print(": command not found\n");
  return 1;
}

static int shell_top_level_operator(const char *line, size_t *position,
                                    size_t *length) {
  int quote = 0;
  int substitution_depth = 0;

  for (size_t i = 0; line[i]; i++) {
    if (line[i] == '"' && substitution_depth == 0)
      quote = !quote;
    if (quote)
      continue;
    if (line[i] == '$' && line[i + 1] == '(') {
      substitution_depth++;
      i++;
      continue;
    }
    if (line[i] == ')' && substitution_depth > 0) {
      substitution_depth--;
      continue;
    }
    if (substitution_depth > 0)
      continue;

    if (line[i] == '&' && line[i + 1] == '&') {
      *position = i;
      *length = 2;
      return 1;
    }
    if (line[i] == '&') {
      *position = i;
      *length = 1;
      return 5;
    }
    if (line[i] == '|' && line[i + 1] == '|') {
      *position = i;
      *length = 2;
      return 2;
    }
    if (line[i] == ';') {
      *position = i;
      *length = 1;
      return 3;
    }
    if (line[i] == '<' && line[i + 1] == '<') {
      *position = i;
      *length = 2;
      return 6;
    }
  }
  return 0;
}

static int shell_execute_stage(char *line, const char *input, size_t input_length,
                               char *output, size_t output_capacity,
                               int show_output) {
  char command[COMMAND_LINE_MAX];
  char input_path[256] = "";
  char output_path[256] = "";
  size_t command_length = 0;
  int append = 0;
  int quote = 0;

  for (size_t i = 0; line[i]; i++) {
    if (line[i] == '"') {
      quote = !quote;
      command[command_length++] = line[i];
      continue;
    }
    if (!quote && (line[i] == '>' || line[i] == '<')) {
      char *path = line[i] == '>' ? output_path : input_path;
      size_t path_length = 0;
      if (line[i] == '>' && line[i + 1] == '>') {
        append = 1;
        i++;
      }
      while (line[i + 1] == ' ' || line[i + 1] == '\t')
        i++;
      while (line[i + 1] && line[i + 1] != ' ' && line[i + 1] != '\t' &&
             line[i + 1] != '|' && line[i + 1] != ';' &&
             line[i + 1] != '&' && path_length < 255)
        path[path_length++] = line[++i];
      path[path_length] = '\0';
      continue;
    }
    if (command_length + 1 >= sizeof(command))
      return 1;
    command[command_length++] = line[i];
  }
  command[command_length] = '\0';

  if (output_path[0] && !input_path[0]) {
    char command_copy[COMMAND_LINE_MAX];
    char *command_argv[CMD_MAX_ARGS];
    int command_argc;

    k_strcp(command_copy, command);
    command_argc = k_split(command_copy, command_argv, CMD_MAX_ARGS);
    if (command_argc == 1 && k_strcmp(command_argv[0], "cat") == 0) {
      if (!shell_begin_input_redirect(output_path, append)) {
        t_print_raw(output_path);
        t_print(": could not open\n");
        return 1;
      }
      return 0;
    }
  }

  char file_input[SHELL_IO_BUFFER_SIZE];
  if (input_path[0]) {
    char resolved[256];
    struct vfs_file file;
    uint32_t read;
    size_t total = 0;
    resolve_path(shell_get_cwd(), input_path, resolved);
    if (!vfs_open(resolved, &file)) {
      t_print_raw(input_path);
      t_print(": not found\n");
      return 1;
    }
    while (total < sizeof(file_input) &&
           (read = vfs_read(&file, file_input + total,
                            sizeof(file_input) - total)) > 0)
      total += read;
    input = file_input;
    input_length = total;
  }

  const char *old_input = shell_input_data;
  size_t old_input_length = shell_input_length;
  shell_input_data = input;
  shell_input_length = input_length;

  if (show_output && !output_path[0]) {
    int status = commands_execute_simple(command);
    shell_input_data = old_input;
    shell_input_length = old_input_length;
    return status;
  }

  t_capture_begin(output, output_capacity);
  int status = commands_execute_simple(command);
  size_t output_length = t_capture_end();
  shell_input_data = old_input;
  shell_input_length = old_input_length;

  if (output_path[0]) {
    char resolved[256];
    struct vfs_file file;
    resolve_path(shell_get_cwd(), output_path, resolved);
    if (!append && vfs_open(resolved, &file)) {
      if (!vfs_remove(resolved)) {
        t_print_raw(output_path);
        t_print(": could not overwrite\n");
        return 1;
      }
    }
    if (!vfs_open(resolved, &file) &&
        (!vfs_create(resolved) || !vfs_open(resolved, &file))) {
      t_print_raw(output_path);
      t_print(": could not open\n");
      return 1;
    }
    if (append)
      file.fat32.offset = file.fat32.size;
    if (vfs_write(&file, output, output_length) != output_length)
      return 1;
    return status;
  }

  if (show_output)
    t_write(output, output_length);
  return status;
}

static int shell_execute_pipeline(char *line) {
  char stage[COMMAND_LINE_MAX];
  char input[SHELL_IO_BUFFER_SIZE];
  char output[SHELL_IO_BUFFER_SIZE];
  size_t input_length = 0;
  size_t stage_length = 0;
  int status = 0;
  int quote = 0;
  int substitution_depth = 0;

  for (size_t i = 0;; i++) {
    char character = line[i];
    if (character == '"' && substitution_depth == 0)
      quote = !quote;
    if (!quote && character == '$' && line[i + 1] == '(') {
      substitution_depth++;
      if (stage_length + 2 >= sizeof(stage))
        return 1;
      stage[stage_length++] = line[i++];
      stage[stage_length++] = line[i];
      continue;
    }
    if (!quote && character == ')' && substitution_depth > 0)
      substitution_depth--;

    if ((character != '|' || quote || substitution_depth > 0) &&
        character != '\0') {
      if (stage_length + 1 >= sizeof(stage))
        return 1;
      stage[stage_length++] = character;
      continue;
    }

    stage[stage_length] = '\0';
    int last = character == '\0';
    status = shell_execute_stage(stage, input, input_length, output,
                                 sizeof(output), last);
    if (!last) {
      for (size_t j = 0; j < sizeof(input) && j < sizeof(output); j++)
        input[j] = output[j];
      input_length = k_strlen(input);
      stage_length = 0;
      continue;
    }
    return status;
  }
}

void commands_execute(char *line) {
  size_t position;
  size_t length;
  int operator_type = shell_top_level_operator(line, &position, &length);

  if (operator_type) {
    char left[COMMAND_LINE_MAX];
    char right[COMMAND_LINE_MAX];
    size_t left_length = position;
    size_t right_length = k_strlen(line) - position - length;

    if (left_length >= sizeof(left) || right_length >= sizeof(right))
      return;
    for (size_t i = 0; i < left_length; i++)
      left[i] = line[i];
    left[left_length] = '\0';
    for (size_t i = 0; i < right_length; i++)
      right[i] = line[position + length + i];
    right[right_length] = '\0';

    if (operator_type == 5) {
      t_print("background execution is not supported\n");
      return;
    }
    if (operator_type == 6) {
      t_print("here-documents are not supported\n");
      return;
    }

    int status = shell_execute_pipeline(left);
    if (operator_type == 1 && status == 0)
      commands_execute(right);
    else if (operator_type == 2 && status != 0)
      commands_execute(right);
    else if (operator_type == 3)
      commands_execute(right);
    return;
  }

  shell_execute_pipeline(line);
}
