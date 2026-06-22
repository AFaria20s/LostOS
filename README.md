# 0x194OS

0x194OS is a small bare-metal x86 operating system written in C.
It boots in QEMU, gives me a shell to play with, and is where I keep building the usual low-level pieces one by one: keyboard input, command handling, screen output, memory, and scrollback.

It is still very much a work in progress, but it already feels like a real system instead of a blank screen with ambition. The fun part is that every command, every color on the screen, and every bit of input handling is running on code from this repo.

## Screenshots

Booting into the shell:

![Boot and welcome screen](docs/screenshots/boot.png)

Some shell commands in action:

![Shell commands](docs/screenshots/shell.png)

## What works right now

- Multiboot boot flow through GRUB
- 32-bit protected mode kernel
- VGA text output with simple inline color codes
- PS/2 keyboard input
- Portuguese and US keyboard layouts
- Interactive shell with editable input
- Command history with timestamps
- Screen scrollback
- A first kernel heap with `kmalloc` / `kfree`
- Basic memory inspection and heap test commands

## Commands

```text
help          list available commands
clear         clear the screen
echo          print text, including color codes like $a, $b, $c and $f
argc          show how many arguments were received
history       show command history
layout        show or change keyboard layout
mem           inspect kernel memory
mem test      run a small heap test
mem test -d   run the heap test with step-by-step output
```

## Dependencies

```bash
sudo apt install gcc nasm qemu-system-x86 grub-pc-bin xorriso make
```

## Running

```bash
git clone https://github.com/AFaria20s/0x194OS
cd 0x194OS
make run
```

## Build ISO only

```bash
make
```

Built by hand, one piece at a time.
