#include <stdint.h>

#include "../include/idt.h"
#include "../include/io.h"
#include "../include/keyboard.h"
#include "../include/vga.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

extern void keyboard_wrapper(void);

static uint8_t pic1_mask = 0xFF;
static uint8_t pic2_mask = 0xFF;

static void pic_set_masks(uint8_t master_mask, uint8_t slave_mask) {
  pic1_mask = master_mask;
  pic2_mask = slave_mask;
  outb(PIC1_DATA, pic1_mask);
  outb(PIC2_DATA, pic2_mask);
}

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

  // Leave only keyboard enabled (PIC mask)
  pic_set_masks(0xFD, 0xFF);
}

// IDT entry structure
struct idt_entry {
  uint16_t base_low;
  uint16_t selector;
  uint8_t zero;
  uint8_t flags;
  uint16_t base_high;
} __attribute__((packed));

// Pointer used by lidt
struct idt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;
static int idt_ready = 0;
static int keyboard_irq_ready = 0;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector,
                  uint8_t flags) {
  idt[num].base_low = base & 0xFFFF;
  idt[num].base_high = (base >> 16) & 0xFFFF;
  idt[num].selector = selector;
  idt[num].zero = 0;
  idt[num].flags = flags;
}

static uint32_t idt_gate_base(uint8_t num) {
  return ((uint32_t)idt[num].base_high << 16) | idt[num].base_low;
}

static int idt_gate_matches(uint8_t num, uint32_t base, uint16_t selector,
                            uint8_t flags) {
  return idt_gate_base(num) == base &&
         idt[num].selector == selector &&
         idt[num].zero == 0 &&
         idt[num].flags == flags;
}

int idt_init(void) {
  struct idt_ptr loaded_idt;

  // Initialize IDT, configure PIC and enable keyboard ISR
  idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
  idtp.base = (uint32_t)idt;

  for (int i = 0; i < 256; i++)
    idt_set_gate(i, 0, 0, 0);

  pic_init();
  idt_set_gate(0x21, (uint32_t)keyboard_wrapper, 0x08, 0x8E);

  __asm__ volatile("lidt %0" : : "m"(idtp) : "memory");
  __asm__ volatile("sidt %0" : "=m"(loaded_idt));

  keyboard_irq_ready = idt_gate_matches(0x21, (uint32_t)keyboard_wrapper,
                                        0x08, 0x8E) &&
                       (pic1_mask & 0x02) == 0;

  idt_ready = loaded_idt.limit == idtp.limit &&
              loaded_idt.base == idtp.base &&
              keyboard_irq_ready;

  if (idt_ready)
    __asm__ volatile("sti");

  return idt_ready;
}

int idt_is_ready(void) {
  return idt_ready;
}

int idt_keyboard_irq_is_ready(void) {
  return keyboard_irq_ready;
}
