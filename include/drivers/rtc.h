#ifndef RTC_H
#define RTC_H

#include <stdint.h>

struct rtc_time {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t day;
  uint8_t month;
  uint8_t year;
};

/* The QEMU RTC is configured as UTC; LostOS presents local Lisbon time. */
#define RTC_LOCAL_UTC_OFFSET_HOURS 1

void rtc_read(struct rtc_time *out);

#endif
