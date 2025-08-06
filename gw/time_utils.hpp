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
    Serial.println("Tab5 RTC Enabled");
    m5::rtc_datetime_t dt;
    if( Tab5Rtc.getDateTime(&dt)) {
      auto tm = dt.get_tm();
      Serial.printf("Got RTC Time: %s\n", formatDateTime(tm).c_str());
      Tab5Rtc.setSystemTimeFromRtc();
      rtc_ok = true;
    } else {
      Serial.println("Failed to get RTC Time");
    }
  } else {
    Serial.println("Tab5 RTC fail");
  }
}


// String serialBuffer = "";
//
// void handleSerialTimeSet()
// {
//   while (Serial.available()) {
//     char c = Serial.read();
//     if (c == '\n' || c == '\r') {
//       if (serialBuffer.length() > 0) {
//         // "2025/06/14 08:30:00" 形式で解析
//         int year, month, day, hour, minute, second;
//         if (sscanf(serialBuffer.c_str(), "%d/%d/%d %d:%d:%d",
//                    &year, &month, &day, &hour, &minute, &second) == 6) {
//
//           if (rtc_ok) {
//             struct tm t;
//             t.tm_year = year - 1900;  // tm_yearは1900年からの年数
//             t.tm_mon = month - 1;     // tm_monは0-11
//             t.tm_mday = day;
//             t.tm_hour = hour;
//             t.tm_min = minute;
//             t.tm_sec = second;
//
//             if (rtc.setTime(t)) {
//               Serial.println("RTC time is now set to: " + formatDateTime(t));
//             } else {
//               Serial.println("Error: RTC time update failed");
//             }
//           } else {
//             Serial.println("Error: RTC was not okay");
//           }
//         } else {
//           Serial.println("Error: bad input format");
//           Serial.println("Format must be: YYYY/MM/DD HH:MM:SS");
//           Serial.println("e.g. 2025/06/14 08:30:00");
//         }
//         serialBuffer = "";
//       }
//     } else {
//       serialBuffer += c;
//     }
//   }
// }
