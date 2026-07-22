CC = gcc
AS = as
CPPFLAGS = -Iinclude
CFLAGS = -m32 -nostdlib -ffreestanding -fno-stack-protector -fno-pic -Wall

KERNEL_C = \
	kernel/kernel.c \
	kernel/arch/gdt.c \
	kernel/arch/idt.c \
	kernel/drivers/ata.c \
	kernel/drivers/keyboard.c \
	kernel/drivers/keyboard_layouts.c \
	kernel/drivers/rtc.c \
	kernel/drivers/vga.c \
	kernel/fs/fat32.c \
	kernel/fs/mbr.c \
	kernel/fs/vfs.c \
	kernel/lib/kstring.c \
	kernel/mm/memory.c \
	kernel/mm/paging.c \
	kernel/shell/commands.c \
	kernel/shell/shell.c \
	kernel/shell/sysinfo.c \
	kernel/editor/editor.c \
	kernel/lib/path.c
KERNEL_ASM = boot/boot.s boot/interrupt.s
LINKER = linker.ld
OBJDIR = obj
KERNEL_OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(KERNEL_C))

all: os.iso

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

kernel.bin: $(KERNEL_ASM) $(KERNEL_OBJS)
	$(AS) --32 boot/boot.s -o boot.o
	$(AS) --32 boot/interrupt.s -o interrupt.o
	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o interrupt.o $(KERNEL_OBJS)

os.iso: kernel.bin
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp boot/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o os.iso isodir

tools/mkdisk: tools/mkdisk.c
	gcc tools/mkdisk.c -o tools/mkdisk

disk.img: tools/mkdisk
	./tools/mkdisk

clean:
	rm -rf *.o *.bin *.iso isodir tools/mkdisk disk.img $(OBJDIR)

run: os.iso disk.img
	qemu-system-x86_64 -boot d -cdrom os.iso -drive file=disk.img,format=raw -m 512M
