#include <stdint.h>

#include "arch/io.h"
#include "drivers/mouse.h"

#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_DATA 0x60

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL  0x02
#define PS2_COMMAND_ENABLE_AUX 0xA8
#define PS2_COMMAND_DISABLE_AUX 0xA7
#define PS2_COMMAND_DISABLE_KBD 0xAD
#define PS2_COMMAND_ENABLE_KBD 0xAE
#define PS2_COMMAND_READ_CONFIG 0x20
#define PS2_COMMAND_WRITE_CONFIG 0x60
#define PS2_MOUSE_WRITE 0xD4
#define PIC1_COMMAND 0x20
#define PIC2_COMMAND 0xA0
#define PIC_EOI 0x20

#define MOUSE_SET_DEFAULTS 0xF6
#define MOUSE_SET_SAMPLE_RATE 0xF3
#define MOUSE_ENABLE 0xF4
#define MOUSE_SAMPLE_RATE_200 200

static uint32_t mouse_x;
static uint32_t mouse_y;
static uint32_t screen_width;
static uint32_t screen_height;
static uint8_t buttons;
static uint8_t packet[3];
static uint8_t packet_index;
static int mouse_ready;
static volatile int mouse_changed;

static void send_eoi(void) {
  outb(PIC2_COMMAND, PIC_EOI);
  outb(PIC1_COMMAND, PIC_EOI);
}

static int wait_for_input(void) {
  for (uint32_t timeout = 0; timeout < 100000; timeout++)
    if (!(inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL))
      return 0;
  return -1;
}

static int wait_for_output(void) {
  for (uint32_t timeout = 0; timeout < 100000; timeout++)
    if (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)
      return 0;
  return -1;
}

static int controller_write(uint8_t value) {
  if (wait_for_input() != 0)
    return -1;
  outb(PS2_COMMAND, value);
  return 0;
}

static int mouse_write(uint8_t value) {
  if (controller_write(PS2_MOUSE_WRITE) != 0 || wait_for_input() != 0)
    return -1;
  outb(PS2_DATA, value);
  return 0;
}

static int mouse_read(uint8_t *value) {
  if (!value || wait_for_output() != 0)
    return -1;
  *value = inb(PS2_DATA);
  return 0;
}

static void discard_output(void) {
  while (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)
    (void)inb(PS2_DATA);
}

static int mouse_set_enabled(int enabled) {
  uint8_t config;

  if (controller_write(PS2_COMMAND_READ_CONFIG) != 0 ||
      mouse_read(&config) != 0)
    return -1;

  if (enabled)
    config = (uint8_t)((config | 0x02) & (uint8_t)~0x30);
  else
    config |= 0x20;

  if (controller_write(PS2_COMMAND_WRITE_CONFIG) != 0 ||
      wait_for_input() != 0)
    return -1;
  outb(PS2_DATA, config);
  return 0;
}

static int mouse_set_sample_rate(uint8_t rate) {
  uint8_t response;

  if (mouse_write(MOUSE_SET_SAMPLE_RATE) != 0 ||
      mouse_read(&response) != 0 || response != 0xFA ||
      mouse_write(rate) != 0 ||
      mouse_read(&response) != 0 || response != 0xFA)
    return -1;

  return 0;
}

int mouse_init(void) {
  uint8_t response;

  mouse_ready = 0;
  packet_index = 0;
  buttons = 0;
  mouse_changed = 0;

  if (controller_write(PS2_COMMAND_DISABLE_AUX) != 0 ||
      controller_write(PS2_COMMAND_DISABLE_KBD) != 0)
    return -1;

  discard_output();

  if (controller_write(PS2_COMMAND_ENABLE_AUX) != 0 ||
      mouse_set_enabled(1) != 0 ||
      controller_write(PS2_COMMAND_ENABLE_KBD) != 0 ||
      mouse_write(MOUSE_SET_DEFAULTS) != 0 ||
      mouse_read(&response) != 0 || response != 0xFA ||
      mouse_set_sample_rate(MOUSE_SAMPLE_RATE_200) != 0 ||
      mouse_write(MOUSE_ENABLE) != 0 ||
      mouse_read(&response) != 0 || response != 0xFA)
    return -1;

  mouse_ready = 1;
  return 0;
}

void mouse_set_bounds(uint32_t width, uint32_t height) {
  screen_width = width;
  screen_height = height;
  mouse_x = width ? width / 2 : 0;
  mouse_y = height ? height / 2 : 0;
  mouse_changed = 1;
}

static void process_byte(uint8_t value) {
  int32_t next_x;
  int32_t next_y;
  int8_t delta_x;
  int8_t delta_y;

  if (packet_index == 0 && !(value & 0x08))
    return;

  packet[packet_index++] = value;
  if (packet_index < 3) {
    return;
  }
  packet_index = 0;

  /* Discard overflowed or desynchronised PS/2 packets. */
  if ((packet[0] & 0xC0) ||
      ((packet[0] & 0x10) != ((packet[1] & 0x80) >> 3)) ||
      ((packet[0] & 0x20) != ((packet[2] & 0x80) >> 2)))
    return;

  buttons = packet[0] & 0x07;
  delta_x = (int8_t)packet[1];
  delta_y = (int8_t)packet[2];
  next_x = (int32_t)mouse_x + delta_x;
  next_y = (int32_t)mouse_y - delta_y;

  if (next_x < 0)
    next_x = 0;
  if (screen_width && next_x >= (int32_t)screen_width)
    next_x = (int32_t)screen_width - 1;
  if (next_y < 0)
    next_y = 0;
  if (screen_height && next_y >= (int32_t)screen_height)
    next_y = (int32_t)screen_height - 1;

  mouse_x = (uint32_t)next_x;
  mouse_y = (uint32_t)next_y;
  mouse_changed = 1;
}

void mouse_handler(void) {
  if (mouse_ready && (inb(PS2_STATUS) & 0x20))
    process_byte(inb(PS2_DATA));
  send_eoi();
}

void mouse_poll(void) {
  if (!mouse_ready)
    return;

  while ((inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) &&
         (inb(PS2_STATUS) & 0x20))
    process_byte(inb(PS2_DATA));
}

uint32_t mouse_get_x(void) { return mouse_x; }
uint32_t mouse_get_y(void) { return mouse_y; }
uint8_t mouse_get_buttons(void) { return buttons; }

int mouse_has_changed(void) {
  int changed = mouse_changed;
  mouse_changed = 0;
  return changed;
}
