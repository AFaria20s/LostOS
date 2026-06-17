CC = gcc
AS = as
CFLAGS = -m32 -nostdlib -ffreestanding -fno-stack-protector -fno-pic -Wall

KERNEL_C = kernel/kernel.c kernel/gdt.c kernel/idt.c kernel/keyboard.c kernel/shell.c kernel/commands.c kernel/kstring.c
KERNEL_ASM = boot/boot.s
LINKER = linker.ld

all: os.iso

kernel.bin: $(KERNEL_ASM) $(KERNEL_C)
	$(AS) --32 boot/boot.s -o boot.o
	$(AS) --32 boot/interrupt.s -o interrupt.o
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o
	$(CC) $(CFLAGS) -c kernel/idt.c -o idt.o
	$(CC) $(CFLAGS) -c kernel/keyboard.c -o keyboard.o
	$(CC) $(CFLAGS) -c kernel/gdt.c -o gdt.o
	$(CC) $(CFLAGS) -c kernel/shell.c -o shell.o
	$(CC) $(CFLAGS) -c kernel/commands.c -o commands.o
	$(CC) $(CFLAGS) -c kernel/kstring.c -o kstring.o
	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o interrupt.o kernel.o gdt.o idt.o keyboard.o shell.o commands.o kstring.o

os.iso: kernel.bin
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp boot/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o os.iso isodir

run: os.iso
	qemu-system-x86_64 -cdrom os.iso

clean:
	rm -rf *.o *.bin *.iso isodir
