#include <stdint.h>

#include "../include/gdt.h"

// GDT entry (Global Descriptor Table)
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// Pointer used by lgdt
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdtp;
static int gdt_ready = 0;

// Set a GDT entry
static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt[num].base_low    = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access      = access;
}

int gdt_init(void) {
    struct gdt_ptr loaded_gdt;

    // Initialize and load GDT with three entries
    gdtp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gdtp.base  = (uint32_t)gdt;

    gdt_set_gate(0, 0, 0,          0x00, 0x00);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        : : "m"(gdtp) : "ax", "memory"
    );

    __asm__ volatile ("sgdt %0" : "=m"(loaded_gdt));

    gdt_ready = loaded_gdt.limit == gdtp.limit &&
                loaded_gdt.base == gdtp.base;

    return gdt_ready;
}

int gdt_is_ready(void) {
    return gdt_ready;
}
