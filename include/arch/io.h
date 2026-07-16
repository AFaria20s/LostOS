#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
  uint8_t val;
  __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
  return val;
}

static inline void outw(uint16_t port, uint16_t val) {
  __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %w1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %w1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#endif
