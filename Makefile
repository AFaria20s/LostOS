CC = gcc
AS = as
CFLAGS = -m32 -nostdlib -ffreestanding -fno-stack-protector -fno-pic -Wall

KERNEL_C = kernel/kernel.c kernel/vga.c kernel/gdt.c kernel/idt.c kernel/keyboard.c kernel/keyboard_layouts.c kernel/shell.c kernel/commands.c kernel/kstring.c kernel/memory.c kernel/paging.c
KERNEL_ASM = boot/boot.s
LINKER = linker.ld

all: os.iso

kernel.bin: $(KERNEL_ASM) $(KERNEL_C)
	$(AS) --32 boot/boot.s -o boot.o
	$(AS) --32 boot/interrupt.s -o interrupt.o
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o
	$(CC) $(CFLAGS) -c kernel/vga.c -o vga.o
	$(CC) $(CFLAGS) -c kernel/idt.c -o idt.o
	$(CC) $(CFLAGS) -c kernel/keyboard.c -o keyboard.o
	$(CC) $(CFLAGS) -c kernel/keyboard_layouts.c -o keyboard_layouts.o
	$(CC) $(CFLAGS) -c kernel/gdt.c -o gdt.o
	$(CC) $(CFLAGS) -c kernel/shell.c -o shell.o
	$(CC) $(CFLAGS) -c kernel/commands.c -o commands.o
	$(CC) $(CFLAGS) -c kernel/kstring.c -o kstring.o
	$(CC) $(CFLAGS) -c kernel/memory.c -o memory.o
	$(CC) $(CFLAGS) -c kernel/paging.c -o paging.o
	$(CC) $(CFLAGS) -c kernel/sysinfo.c -o sysinfo.o
	$(CC) $(CFLAGS) -c kernel/ata.c -o ata.o
	$(CC) $(CFLAGS) -c kernel/mbr.c -o mbr.o
	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o interrupt.o kernel.o vga.o gdt.o idt.o keyboard.o keyboard_layouts.o shell.o commands.o kstring.o memory.o paging.o sysinfo.o ata.o mbr.o

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
	rm -rf *.o *.bin *.iso isodir tools/mkdisk disk.img

run: os.iso disk.img
	qemu-system-x86_64 -boot d -cdrom os.iso -drive file=disk.img,format=raw -m 512M
