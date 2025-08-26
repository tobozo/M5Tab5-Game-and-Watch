/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#pragma once

#include "./m5unified-rtc.hpp"

m5::RTC8130_Class Tab5Rtc;

bool rtc_ok = false;

#define RTC_MEM_VOLUME_ADDR 0x0 // 2 bytes
#define RTC_MEM_GAMEID_ADDR 0x2 // 1 byte
#define RTC_MEM_BRIGHT_ADDR 0x3 // 1 byte

String formatDateTime(struct tm &t)
{
  char buffer[255];
  sprintf(buffer, "%04d/%02d/%02d %02d:%02d:%02d",
    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
    t.tm_hour, t.tm_min, t.tm_sec);
  return String(buffer);
}


time_t parseDateTime(const String& dt)
{
  int y, M, d, h, m;
  sscanf(dt.c_str(), "%d/%d/%d %d:%d", &y, &M, &d, &h, &m);
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon  = M - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min  = m;
  t.tm_sec  = 0;
  return mktime(&t);
}



void initRTC()
{
  if(Tab5Rtc.begin()) {
    //Serial.println("Tab5 RTC Enabled");
    m5::rtc_datetime_t dt;
    if( Tab5Rtc.getDateTime(&dt)) {
      auto tm = dt.get_tm();
      ESP_LOGI("Tab5RTC", "Got RTC Time: %s\n", formatDateTime(tm).c_str());
      Tab5Rtc.setSystemTimeFromRtc();
      rtc_ok = true;
    } else {
      ESP_LOGE("Tab5RTC", "Failed to get RTC Time");
    }
  } else {
    ESP_LOGE("Tab5RTC", "Tab5 RTC fail");
  }
}

