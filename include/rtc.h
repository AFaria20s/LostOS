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

void rtc_read(struct rtc_time *out);

#endif