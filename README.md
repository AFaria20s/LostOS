# Lost OS

Lost OS is a bare-metal x86 operating system written in C.
It boots in QEMU and on real hardware, gives you an interactive shell, and builds a complete low-level stack from disk access to a text editor.

Every command, every color on screen, every keyboard event and every file read from disk is handled by code in this repository.

## Screenshots

Booting into the shell:

![Boot and welcome screen](docs/screenshots/boot.png)

Some shell commands in action:

![Shell commands](docs/screenshots/shell.png)

## Highlights

- Fully custom 32-bit protected-mode kernel
- VGA text output with inline color codes (`$a`, `$b`, `$c`, ...)
- PS/2 keyboard support with Portuguese and US layouts
- Interactive shell with cursor movement, history and autocomplete
- ATA PIO storage driver for legacy IDE disks
- MBR partition parsing and FAT32 filesystem reader
- VFS abstraction layer that exposes files and directories to user commands
- Integrated `lost` text editor with file editing and save support

## Lost editor

The `lost` editor is the main feature of this release.
It runs on top of the filesystem layer, opens files from disk, and lets you edit them with keyboard-driven modes.

Key editor features:

- `COMMAND` mode for navigation and editor commands
- `INSERT` mode for typing text
- Bottom status bar showing current mode and file path
- `:w` to save file changes to disk
- `:q` to quit editor mode
- `:wq` to save and exit
- horizontal scrolling for long lines
- line insertion, deletion and navigation with arrow keys

Use the editor like this:

```text
lost <path>
```

## What works right now

- Multiboot boot flow through GRUB
- GDT and IDT
- VGA text output and hardware cursor control
- PS/2 keyboard input and modifiers
- Shell with editable input, history, and tab completion
- Kernel heap allocator (`kmalloc` / `kfree`)
- Basic paging and memory tools
- ATA PIO read support
- MBR partition table parsing
- FAT32 read-only driver for file and directory access
- VFS abstraction layer over FAT32
- Built-in editor `lost` for file editing
- Shell commands for listing, reading, creating and removing files
- `whatami` system information command

## Commands

```text
help          list available commands
clear         clear the screen
echo          print text with color codes
argc          show how many arguments were received
history       show command history with timestamps
sudo          secret command (no help text)
layout        show or set keyboard layout
mem           shows kernel memory usage
paging        shows paging status
whatami       what are you exactly?
atatest       test ATA, MBR and FAT32
ls            list the files in the directory
read          display the content of the file
touch         create an empty file
mkdir         create a directory
rm            remove a file
cp            copy a file
mv            move or rename a file
rmdir         remove an empty directory
lost <path>   open Lost text editor
```

## Dependencies

```bash
sudo apt install gcc qemu-system-x86 grub-pc-bin xorriso make
```

## Running

```bash
git clone https://github.com/AFaria20s/LostOS
cd LostOS
make run
```

The disk image is created automatically on first run.

## Build ISO only

```bash
make
```

## Notes

- The ATA driver uses legacy PIO mode (ports 0x1F0-0x1F7).
- **QEMU**: works out of the box
- **VirtualBox**: set the storage controller to IDE (PIIX4), not SATA/AHCI
- **Real hardware**: works if BIOS has IDE compatibility mode enabled

Built by hand, one piece at a time.