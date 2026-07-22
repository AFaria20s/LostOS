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

## Architecture

```text
GRUB
  └── Kernel
      ├── GDT / IDT / Paging / Heap
      ├── VGA / PS2 keyboard / RTC
      ├── ATA PIO → MBR → FAT32 → VFS
      └── Shell → commands → lost editor
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

The shell keeps track of a current working directory. Filesystem commands accept absolute and relative paths, including `.` and `..`.

```text
404@LostOS:/$ cd docs
404@LostOS:/docs$ read info.txt
404@LostOS:/docs$ cd ..
404@LostOS:/$ ls ./docs
```

Tab completes commands at the beginning of a line, and file or directory names in arguments. When more than one option is available, the shell prints the matches: directories are light blue and end with `/`; files are white.

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
| `make clean` | Remove generated build artifacts |

## Storage and current limitations

Disk access uses ATA PIO through the legacy IDE ports `0x1F0-0x1F7`. QEMU works without extra configuration. VirtualBox and physical hardware may need an IDE compatibility mode.

The filesystem currently supports FAT 8.3 short names. The shell displays names in lowercase and lookups are case-insensitive, but FAT long file names (LFN) and original filename casing are not preserved yet.

## License

Released under the [MIT License](LICENSE).
