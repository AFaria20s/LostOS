#include "arch/io.h"
#include "drivers/rtc.h"

static uint8_t rtc_read_register(uint8_t reg) {
  outb(0x70, reg);
  return inb(0x71);
}

static int rtc_is_updating(void) {
  return (rtc_read_register(0x0A) & 0x80) != 0;
}

static uint8_t bcd_to_binary(uint8_t value) {
  return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

void rtc_read(struct rtc_time *out) {
  uint8_t status;
  uint8_t raw_hour;
  int pm;

  if (!out)
    return;
  while (rtc_is_updating()) {}
  out->second = rtc_read_register(0x00);
  out->minute = rtc_read_register(0x02);
  raw_hour = rtc_read_register(0x04);
  out->day = rtc_read_register(0x07);
  out->month = rtc_read_register(0x08);
  out->year = rtc_read_register(0x09);
  status = rtc_read_register(0x0B);
  pm = (raw_hour & 0x80) != 0;
  out->hour = (uint8_t)(raw_hour & 0x7F);

  if (!(status & 0x04)) {
    out->second = bcd_to_binary(out->second);
    out->minute = bcd_to_binary(out->minute);
    out->hour = bcd_to_binary((uint8_t)(out->hour & 0x7F));
    out->day = bcd_to_binary(out->day);
    out->month = bcd_to_binary(out->month);
    out->year = bcd_to_binary(out->year);
  }

  if (!(status & 0x02)) {
    if (pm && out->hour < 12)
      out->hour = (uint8_t)(out->hour + 12);
    else if (!pm && out->hour == 12)
      out->hour = 0;
  }
}
