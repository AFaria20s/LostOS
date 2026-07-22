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
#include "editor/editor.h"
#include "lib/path.h"

// Maximum arguments per command
#define CMD_MAX_ARGS 16

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
};

static const int command_count = sizeof(commands) / sizeof(commands[0]);

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
      if (entry.attributes & 0x10)
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

void commands_execute(char *line) {
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
