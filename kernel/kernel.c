#include <stddef.h>
#include <stdint.h>

#include "arch/gdt.h"
#include "arch/idt.h"
#include "mm/memory.h"
#include "mm/paging.h"
#include "shell/shell.h"
#include "drivers/vga.h"
#include "drivers/ata.h"
#include "drivers/keyboard.h"
#include "fs/vfs.h"
#include "fs/first_boot.h"
#include "fs/config.h"
#ifdef GUI
#include "drivers/fb.h"
#include "drivers/mouse.h"
#include "gui/gui.h"
#endif

static const char *old_boot_logo =
" /$$        /$$$$$$   /$$$$$$  /$$$$$$$$        /$$$$$$   /$$$$$$ \n"
"| $$       /$$__  $$ /$$__  $$|__  $$__/       /$$__  $$ /$$__  $$ \n"
"| $$      | $$  \\ $$| $$  \\__/   | $$         | $$  \\ $$| $$  \\__/\n"
"| $$      | $$  | $$|  $$$$$$    | $$         | $$  | $$|  $$$$$$ \n"
"| $$      | $$  | $$ \\____  $$   | $$         | $$  | $$ \\____  $$\n"
"| $$      | $$  | $$ /$$  \\ $$   | $$         | $$  | $$ /$$  \\ $$\n"
"| $$$$$$$$|  $$$$$$/|  $$$$$$/   | $$         |  $$$$$$/|  $$$$$$/\n"
"|________/ \\______/  \\______/    |__/          \\______/  \\______/ \n";

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
  t_print("$fLostOS OS $8/ $b32-bit bare metal shell$f\n");
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
  int gdt_ok = gdt_init();
  int paging_ok = 0;
  int idt_ok = 0;

  if (gdt_ok)
    paging_ok = paging_init();

#ifdef GUI
  int mouse_ok = 0;
  if (gdt_ok && paging_ok)
    mouse_ok = mouse_init() == 0;
#endif

  t_init();
  memory_init(multiboot_magic, multiboot_info_addr);
  ata_init();
  vfs_init();

  if (vfs_is_ready()) {
    first_boot_setup();
    shell_set_cwd("/home");
    shell_history_init();
  }

  config_init();

  if (gdt_ok && paging_ok)
    idt_ok = idt_init();

#ifdef GUI
  int framebuffer_ok = 0;

  if (multiboot_magic == MULTIBOOT_BOOTLOADER_MAGIC &&
      fb_init((multiboot_info_t *)(uintptr_t)multiboot_info_addr) == 0)
    framebuffer_ok = paging_map_range(
      (uint32_t)fb_get_address(), fb_get_pitch() * fb_get_height(),
      PAGING_PAGE_PRESENT | PAGING_PAGE_WRITABLE) == 0;

  if (!framebuffer_ok)
    t_init();
  else {
    vga_disable_cursor();
    welcome_screen();
    if (idt_ok)
      shell_prompt();
    else if (!paging_ok)
      t_print("\n$cBoot stopped: paging is not ready.$f\n");
    else
      t_print("\n$cBoot stopped: interrupts are not ready.$f\n");

    gui_init();
    gui_render_full();
    (void)t_take_dirty();
    for (;;) {
      if (t_take_dirty())
        gui_render_full();
      if (mouse_ok) {
        int pointer_changed;
        int window_changed;

        /* IRQ12 consumes PS/2 bytes; polling concurrently loses packet sync. */
        if (!idt_mouse_irq_is_ready())
          mouse_poll();
        pointer_changed = mouse_has_changed();
        if (pointer_changed) {
          window_changed = gui_handle_pointer(mouse_get_x(), mouse_get_y(),
                                              mouse_get_buttons());
          if (window_changed) {
            if (window_changed == 2)
              gui_render_damage();
            else
              gui_render_full();
          }
          else
            gui_render();
        }
      }
      __asm__ volatile("pause");
    }
  }
#endif

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
