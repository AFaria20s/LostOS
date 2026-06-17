#include <stdint.h>

#include "../include/io.h"
#include "../include/keyboard.h"
#include "../include/vga.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

extern void keyboard_wrapper(void);

void pic_init(void) {
  outb(PIC1_COMMAND, 0x11);
  outb(PIC2_COMMAND, 0x11);

  // Remaps IRQs to 0x20-0x2F.
  outb(PIC1_DATA, 0x20);
  outb(PIC2_DATA, 0x28);

  outb(PIC1_DATA, 0x04);
  outb(PIC2_DATA, 0x02);

  outb(PIC1_DATA, 0x01);
  outb(PIC2_DATA, 0x01);

  // Leaves only the keyboard enabled.
  outb(PIC1_DATA, 0xFD);
  outb(PIC2_DATA, 0xFF);
}

struct idt_entry {
  uint16_t base_low;
  uint16_t selector;
  uint8_t zero;
  uint8_t flags;
  uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector,
                  uint8_t flags) {
  idt[num].base_low = base & 0xFFFF;
  idt[num].base_high = (base >> 16) & 0xFFFF;
  idt[num].selector = selector;
  idt[num].zero = 0;
  idt[num].flags = flags;
}

void idt_init(void) {
  idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
  idtp.base = (uint32_t)&idt;

  for (int i = 0; i < 256; i++)
    idt_set_gate(i, 0, 0, 0);

  pic_init();
  idt_set_gate(0x21, (uint32_t)keyboard_wrapper, 0x08, 0x8E);

  __asm__ volatile("lidt %0" : : "m"(idtp));
  __asm__ volatile("sti");
}
