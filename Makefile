CC = gcc
AS = as
CPPFLAGS = -Iinclude
CFLAGS = -m32 -nostdlib -ffreestanding -fno-stack-protector -fno-pic -Wall -g
TEST_CFLAGS = -Wall -Wextra -Werror -Iinclude
MODE ?= vga

ifeq ($(MODE),gui)
CPPFLAGS += -DGUI
ASFLAGS = --32 --defsym GUI=1
OBJDIR = obj-gui
GRUB_CFG = boot/grub-gui.cfg
else
ASFLAGS = --32
OBJDIR = obj
GRUB_CFG = boot/grub.cfg
endif

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
	kernel/lib/path.c \
	kernel/fs/first_boot.c \
	kernel/fs/config.c
ifeq ($(MODE),gui)
KERNEL_C += kernel/drivers/fb.c
KERNEL_C += kernel/drivers/mouse.c
KERNEL_C += kernel/gui/gui.c
KERNEL_C += kernel/gui/icons.c
KERNEL_C += kernel/gui/theme.c
KERNEL_C += kernel/gui/font.c
endif
KERNEL_ASM = boot/boot.s boot/interrupt.s
ifeq ($(MODE),gui)
KERNEL_ASM += boot/mouse_interrupt.s
endif
LINKER = linker.ld
KERNEL_OBJS = $(patsubst %.c,$(OBJDIR)/%.o,$(KERNEL_C))
KERNEL_ASM_OBJS = $(OBJDIR)/boot.o $(OBJDIR)/interrupt.o
ifeq ($(MODE),gui)
KERNEL_ASM_OBJS += $(OBJDIR)/mouse_interrupt.o
endif

all: os.iso

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/boot.o: boot/boot.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJDIR)/interrupt.o: boot/interrupt.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(OBJDIR)/mouse_interrupt.o: boot/mouse_interrupt.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

kernel.bin: $(KERNEL_ASM) $(KERNEL_ASM_OBJS) $(KERNEL_OBJS) FORCE
	ld -m elf_i386 -T linker.ld -o kernel.bin $(KERNEL_ASM_OBJS) $(KERNEL_OBJS)

os.iso: kernel.bin boot/grub.cfg $(GRUB_CFG)
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp boot/grub.cfg isodir/boot/grub/grub.cfg
	if [ "$(MODE)" = "gui" ]; then cp $(GRUB_CFG) isodir/boot/grub/grub.cfg; fi
	grub-mkrescue -o os.iso isodir

iso: os.iso

iso-vga:
	$(MAKE) MODE=vga os.iso

iso-gui:
	$(MAKE) MODE=gui os.iso

tools/mkdisk: tools/mkdisk.c
	gcc tools/mkdisk.c -o tools/mkdisk

test-kstring: kernel/lib/kstring.c include/lib/kstring.h tests/kstring_test.c
	$(CC) $(TEST_CFLAGS) kernel/lib/kstring.c tests/kstring_test.c -o $@

test: test-kstring
	./test-kstring

disk.img: tools/mkdisk
	./tools/mkdisk

clean:
	rm -rf *.o *.bin *.iso isodir tools/mkdisk disk.img test-kstring obj obj-gui

run:
	$(MAKE) run-vga

run-vga:
	$(MAKE) clean
	$(MAKE) MODE=vga os.iso disk.img
	qemu-system-x86_64 -boot d -cdrom os.iso -drive file=disk.img,format=raw -m 512M

run-gui:
	$(MAKE) clean
	$(MAKE) MODE=gui os.iso disk.img
	qemu-system-x86_64 -boot d -cdrom os.iso -drive file=disk.img,format=raw -m 512M

debug: os.iso disk.img
	qemu-system-x86_64 -s -S -cdrom os.iso -drive file=disk.img,format=raw -m 512M &
	sleep 1
	gdb kernel.bin -ex "set architecture i386" -ex "target remote localhost:1234"

.PHONY: all iso iso-vga iso-gui test test-kstring run run-vga run-gui debug clean FORCE

FORCE:
