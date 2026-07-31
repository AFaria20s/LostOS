# LostOS

LostOS is a 32-bit bare-metal x86 operating system, written mostly in C. It boots through GRUB, brings up its own kernel, and provides an interactive shell, FAT32 storage support, and a VGA text-mode editor.

## Project status

LostOS is still under active development. It is meant for learning and experimentation rather than everyday use. QEMU is currently the main target; legacy hardware interfaces are used deliberately to keep the implementation understandable.

| Area | Status |
| --- | --- |
| GRUB / Multiboot boot flow | Working |
| 32-bit protected-mode x86 kernel | Working |
| VGA, keyboard, and shell | Working |
| ATA PIO, MBR, and FAT32 | Working |
| File creation, editing, and removal | Working |
| First-boot filesystem setup | Working |
| Configurable shell prompt | Working |
| Shell scripts (`/bin/*.lts`) | Working |
| FAT long file names (LFN) | Not supported yet |
| Modern hardware support | Experimental / limited |

## Screenshots

![Boot screen](/docs/screenshots/boot.png)

![Command examples](/docs/screenshots/shell.png)

![Lost text editor](/docs/screenshots/losteditor.png)

## Features

- Multiboot boot flow through GRUB.
- 32-bit protected-mode x86 kernel with GDT, IDT, and paging.
- Kernel allocator with `kmalloc` and `kfree`.
- VGA text output, hardware cursor support, colours, and scrollback.
- PS/2 keyboard driver with `pt` and `us` layouts.
- Shell with editable input, timestamped history, cursor navigation, and autocomplete.
- Linux-style prompt showing the current directory: `404@LostOS:/path$`.
- ATA PIO driver, MBR parsing, and a VFS layer on top of FAT32.
- File and directory operations with relative and absolute paths.
- Built-in `lost` text editor with command and insert modes.
- Automatic first-boot creation of `/etc`, `/home`, `/bin`, and `/docs`.
- Persistent shell configuration in `/etc/lost.cfg`, including a configurable prompt.
- Simple line-based scripts: commands in `/bin/<name>.lts` can be run as `<name>`.

## Architecture

```text
GRUB
  └── Kernel
      ├── GDT / IDT / Paging / Heap
      ├── VGA / PS2 keyboard / RTC
      ├── ATA PIO → MBR → FAT32 → VFS
      └── Shell → commands / .lts scripts → lost editor
```

| Directory | Contents |
| --- | --- |
| `boot/` | Boot code, interrupts, and GRUB configuration |
| `kernel/arch/` | GDT, IDT, and architecture-specific code |
| `kernel/drivers/` | VGA, keyboard, RTC, and ATA drivers |
| `kernel/fs/` | MBR, FAT32, and VFS |
| `kernel/mm/` | Memory management and paging |
| `kernel/shell/` | Shell, commands, and system information |
| `kernel/editor/` | The `lost` text editor |
| `kernel/lib/` | String helpers and path resolution |
| `tools/` | Development tools, including the FAT32 disk-image builder |

## Shell and paths

The shell keeps track of a current working directory, initially `/home` on a mounted disk. Filesystem commands accept absolute and relative paths, including `.` and `..`.

```text
lost@lostos:/home$ cd docs
lost@lostos:/home/docs$ read info.txt
lost@lostos:/home/docs$ cd ..
lost@lostos:/home$ ls ./docs
```

Tab completes commands at the beginning of a line, and file or directory names in arguments. When more than one option is available, the shell prints the matches: directories are light blue and end with `/`; files are white.

### First boot and shell configuration

New development disk images start empty. On the first successful FAT32 mount, LostOS creates `/etc`, `/home`, `/bin`, and `/docs`. It also creates `/etc/lost.cfg` if it is missing:

```ini
username=$2lost
hostname=$alostos
prompt=$u@$h:$p$␠
theme=default
```

The prompt template supports `%u` (username), `%h` (hostname), `%p` (current path), and `%%` (a literal percent sign). `␠` above represents one trailing space after `$`; it is not written to the actual configuration file. Values are rendered with the shell's normal colour markup, so the default `$2` and `$a` prefixes colour the username and hostname. Changes to the file apply after the next boot.

### Scripts

Create a text file named `/bin/<name>.lts`; each non-empty line is executed as a shell command. Invoke it by its name, without the path or extension.

```text
lost@lostos:/home$ lost /bin/hello.lts
echo Hello from LostOS
pwd
:wq
lost@lostos:/home$ hello
Hello from LostOS
/home
```

## Available commands

| Command | Description |
| --- | --- |
| `help` | List available commands |
| `clear` | Clear the screen |
| `echo <text>` | Print text |
| `argc ...` | Show the number of received arguments |
| `history` | Show command history with timestamps |
| `layout [pt\|us]` | Show or change the keyboard layout |
| `mem` | Show kernel memory statistics |
| `mem test [-d [fail]]` | Run allocator tests |
| `paging` | Show virtual-memory status and mapping details |
| `paging test` | Run the paging test |
| `whatami` | Show system information |
| `atatest` | Diagnose ATA, MBR, and FAT32 |
| `pwd` | Print the current directory |
| `cd [path]` | Change directory; with no argument, return to `/` |
| `ls [path]` | List files and directories |
| `read <path>` | Print a file's contents |
| `touch <path>` | Create an empty file |
| `mkdir <path>` | Create a directory |
| `rm <path>` | Remove a file |
| `rmdir <path>` | Remove an empty directory |
| `cp <source> <destination>` | Copy a file |
| `mv <source> <destination>` | Move or rename a file or directory |
| `lost <path>` | Open a file in the built-in editor |
| `tree <path>` | Display the path structure |
 
## The `lost` editor

The editor opens and saves files through the VFS. If the file does not exist yet, it is created the first time it is saved.

```text
lost notes.txt
```

| Key / command | Action |
| --- | --- |
| `i` | Enter insert mode |
| `Esc` | Return to command mode |
| Arrow keys | Move the cursor |
| `:w` | Save the file |
| `:q` | Close the editor |
| `:wq` | Save and close |

## Requirements

The development flow targets Linux. On Debian, Ubuntu, and related distributions:

```bash
sudo apt install build-essential qemu-system-x86 grub-pc-bin xorriso make
```

You will need a compiler that can produce 32-bit x86 code, GNU `as` and `ld`, GRUB with `grub-mkrescue`, `xorriso`, and QEMU.

## Build and run

```bash
git clone https://github.com/AFaria20s/LostOS.git
cd LostOS
make run
```

`make run` builds the kernel, creates the boot ISO, creates an example FAT32 `disk.img`, and starts QEMU.

| Command | Result |
| --- | --- |
| `make` | Create `os.iso` |
| `make disk.img` | Create the development FAT32 disk image |
| `make run` | Build and start LostOS in QEMU |
| `make debug` | Start QEMU paused with a GDB server on port `1234`, then attach GDB to `kernel.bin` |
| `make clean` | Remove generated build artifacts |

## Storage and current limitations

Disk access uses ATA PIO through the legacy IDE ports `0x1F0-0x1F7`. QEMU works without extra configuration. VirtualBox and physical hardware may need an IDE compatibility mode.

The filesystem currently supports FAT 8.3 short names. The shell displays names in lowercase and lookups are case-insensitive, but FAT long file names (LFN) and original filename casing are not preserved yet.

## License

Released under the [MIT License](LICENSE).
