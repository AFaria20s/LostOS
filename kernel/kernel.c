#include <stddef.h>
#include <stdint.h>

#include "../include/gdt.h"
#include "../include/idt.h"
#include "../include/keyboard.h"
#include "../include/memory.h"
#include "../include/paging.h"
#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/ata.h"
#include "../include/vfs.h"

static const char *old_boot_logo =
"  ___           _    ___    _  _       ___  ____  \n"
" / _ \\  __  __ / |  / _ \\  | || |     / _ \\/ ___| \n"
"| | | | \\ \\/ / | | | (_) | | || |_   | | | \\___ \\  \n"
"| |_| |  >  <  | |  \\__, | |__   _|  | |_| |___) | \n"
" \\___/  /_/\\_\\ |_|    /_/     |_|     \\___/|____/  \n";

static void print_boot_status(const char *label, int ok) {
  t_print(ok ? "$a[ OK ]$f " : "$c[ FAIL ]$f ");
  t_print_raw(label);
  t_putchar('\n');
}

static void welcome_screen(void) {
  t_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
  t_putchar('\n');
  t_print_raw(old_boot_logo);

  t_print("\n");
  t_print("$f0x194 OS $8/ $b32-bit bare metal shell$f\n");
  t_print("$8Build: multiboot, VGA text, PS/2 keyboard, kernel heap$f\n\n");

  t_print("$bBoot status$f\n");
  print_boot_status("GDT loaded", gdt_is_ready());
  print_boot_status("IDT loaded", idt_is_ready());
  print_boot_status("Keyboard driver ready", keyboard_is_ready());
  print_boot_status("Keyboard IRQ ready", idt_keyboard_irq_is_ready());
  print_boot_status("Kernel heap ready", memory_is_ready());
  print_boot_status("Paging enabled", paging_is_enabled());
  print_boot_status("ATA drive detected", ata_is_ready());
  if (!ata_is_ready())
    t_print("$8  hint: switch storage controller to IDE$f\n");
  print_boot_status("FAT32 mounted", vfs_is_ready());

  t_print("\n$bUseful commands$f\n");
  t_print("help        list commands\n");
  t_print("mem         inspect kernel memory\n");
  t_print("paging      inspect virtual memory\n");
  t_print("mem test -d run detailed heap test\n");
  t_print("layout      show or change keyboard layout\n");
  t_print("history     show command history\n\n");
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
  t_init();
  memory_init(multiboot_magic, multiboot_info_addr);
  int gdt_ok = gdt_init();
  int paging_ok = 0;
  int idt_ok = 0;

  if (gdt_ok)
    paging_ok = paging_init();

  if (gdt_ok && paging_ok)
    idt_ok = idt_init();

  ata_init();
  vfs_init();

  vga_enable_cursor(14, 15);

  welcome_screen();

  if (idt_ok)
    shell_prompt();
  else if (!paging_ok)
    t_print("\n$cBoot stopped: paging is not ready.$f\n");
  else
    t_print("\n$cBoot stopped: interrupts are not ready.$f\n");

  for (;;)
    ;
}
