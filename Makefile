C = gcc
AS = as
CFLAGS = -m32 -nostdlib -ffreestanding -fno-stack-protector -fno-pic -Wall

KERNEL_C = kernel/kernel.c
KERNEL_ASM = boot/boot.s
LINKER = linker.ld

all: os.iso

kernel.bin: $(KERNEL_ASM) $(KERNEL_C)
	$(AS) --32 $(KERNEL_ASM) -o boot.o
	$(CC) $(CFLAGS) -c $(KERNEL_C) -o kernel.o
	ld -m elf_i386 -T $(LINKER) -o kernel.bin boot.o kernel.o

os.iso: kernel.bin
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp boot/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o os.iso isodir

run: os.iso
	qemu-system-x86_64 -cdrom os.iso

clean:
	rm -rf *.o *.bin *.iso isodir
