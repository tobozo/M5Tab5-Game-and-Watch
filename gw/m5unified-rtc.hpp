/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#pragma once

#include <Wire.h>
#include <M5Unified.hpp>

namespace m5
{
  // types/functions borrowed from utility/RTC8563_Class.hpp:
  //   rtc_time_t, rtc_date_t and rtc_datetime_t
  //   tm rtc_datetime_t::get_tm(void)
  //   void rtc_datetime_t::set_tm(tm& datetime)

  class RTC8130_Class : public I2C_Device
  {
  public:
    static constexpr std::uint8_t DEFAULT_ADDRESS = 0x32;

    RTC8130_Class(std::uint8_t i2c_addr = DEFAULT_ADDRESS, std::uint32_t freq = 400000, I2C_Class* i2c = &In_I2C)
    : I2C_Device ( i2c_addr, freq, i2c )
    {}

    bool begin(I2C_Class* i2c = nullptr);

    bool writeRAM(uint8_t address, uint8_t value) ;
    size_t writeRAM(uint8_t address, uint8_t *value, size_t len) ;
    bool readRAM(uint8_t address, uint8_t *value, size_t len) ;
    uint8_t readRAM(uint8_t address) ;


    bool getDateTime(rtc_datetime_t* datetime) const;
    rtc_datetime_t getDateTime(void) const
    {
      rtc_datetime_t res;
      getDateTime(&res);
      return res;
    }

    void setTime(const rtc_time_t &time);
    void setTime(const rtc_time_t* const time) { if (time) { setTime(*time); } }

    void setDateTime(const rtc_datetime_t &datetime);
    void setDateTime(const rtc_datetime_t* const datetime) { if (datetime) { setDateTime(*datetime); } }
    void setDateTime(const tm* const datetime)
    {
      if (datetime) {
        rtc_datetime_t dt { *datetime };
        setDateTime(dt);
      }
    }

    void setSystemTimeFromRtc(struct timezone* tz = nullptr);


  protected:
    bool stop(bool stop);
  };




  #define LGFX_CHECK_OK(OP) { if (!(OP)) { return false; } }

  static std::uint8_t bcd2ToByte(std::uint8_t value)
  {
    return ((value >> 4) * 10) + (value & 0x0F);
  }

  static std::uint8_t byteToBcd2(std::uint8_t value)
  {
    std::uint_fast8_t bcdhigh = value / 10;
    return (bcdhigh << 4) | (value - (bcdhigh * 10));
  }


  bool RTC8130_Class::stop(bool stop)
  {
    return writeRegister8(0x1E, stop ? 0x040 : 0x00);
  }


  // registers documentation grabbed from ArtronShop_RX8130CE (https://github.com/ArtronShop/ArtronShop_RX8130CE)
  bool RTC8130_Class::begin(I2C_Class* i2c)
  {
    if (i2c)
    {
      _i2c = i2c;
      i2c->begin();
    }

    _init = false;

    /*
     * Digital offset register:
     *   [7]   DET: 0 ->  disabled
     *   [6:0] L7-L1: 0 -> no offset
     */
    LGFX_CHECK_OK(writeRegister8(0x30, 0x00));

    /*
     * Extension Register register:
     *   [7:6]   FSEL: 0 ->  0
     *   [5]     USEL: 0 -> 0
     *   [4]     TE: 0 ->
     *   [3]     WADA: 0 -> 0
     *   [2-0]   TSEL: 0 -> 0
     */
    LGFX_CHECK_OK(writeRegister8(0x1C, 0x00));

    /*
     * Flag Register register:
     *   [7]     VBLF: 0 ->  0
     *   [6]     0: 0 ->
     *   [5]     UF: 0 ->
     *   [4]     TF: 0 ->
     *   [3]     AF: 0 -> 0
     *   [2]     RSF: 0 -> 0
     *   [1]     VLF: 0 -> 0
     *   [0]     VBFF: 0 -> 0
     */
    LGFX_CHECK_OK(writeRegister8(0x1D, 0x00));


    /*
     * Control Register0 register:
     *   [7]     TEST: 0 ->  0
     *   [6]     STOP: 0 ->
     *   [5]     UIE: 0 ->
     *   [4]     TIE: 0 ->
     *   [3]     AIE: 0 -> 0
     *   [2]     TSTP: 0 -> 0
     *   [1]     TBKON: 0 -> 0
     *   [0]     TBKE: 0 -> 0
     */
    LGFX_CHECK_OK(writeRegister8(0x1E, 0x00));

    /*
     * Control Register1 register:
     *   [7-6]   SMPTSEL: 0 ->  0
     *   [5]     CHGEN: 0 ->
     *   [4]     INIEN: 0 ->
     *   [3]     0: 0 ->
     *   [2]     RSVSEL: 0 -> 0
     *   [1-0]   BFVSEL: 0 -> 0
     */
    LGFX_CHECK_OK(writeRegister8(0x1F, 0x00));

    LGFX_CHECK_OK(stop(false)); // clear STOP bit

    /*
     * Function register:
     *   [7]   100TH: 0 ->  disabled
     *   [6:5] Periodic interrupt: 0 -> no periodic interrupt
     *   [4]   RTCM: 0 -> real-time clock mode
     *   [3]   STOPM: 0 -> RTC stop is controlled by STOP bit only
     *   [2:0] Clock output frequency: 000 (Default value)
     */
    LGFX_CHECK_OK(writeRegister8(0x28, 0x00));

    // Battery switch register
    LGFX_CHECK_OK(writeRegister8(0x26, 0x00)); // enable battery switch feature

    _init = true;

    return _init;
  }


  bool RTC8130_Class::writeRAM(uint8_t address, uint8_t value)
  {
    return writeRAM(address, &value, 1);
  }


  size_t RTC8130_Class::writeRAM(uint8_t address, uint8_t *value, size_t len)
  {
    if (address > 3) { // Oversize of 64-bytes RAM
      return 0;
    }

    if ((address + len) > 3) { // Data size over RAM size
      len = 3 - address;
    }

    if (!writeRegister(0x20 + address, value, len)) {
      return 0;
    }

    return len;
  }


  bool RTC8130_Class::readRAM(uint8_t address, uint8_t *value, size_t len)
  {
    if (address > 3) { // Oversize of 64-bytes RAM
      return false;
    }

    if ((address + len) > 3) { // Data size over RAM size
      len = 3 - address;
    }

    return readRegister(0x20 + address, value, len);
  }


  uint8_t RTC8130_Class::readRAM(uint8_t address)
  {
    uint8_t value = 0xFF;
    readRAM(address, &value, 1);
    return value;
  }


  bool RTC8130_Class::getDateTime(rtc_datetime_t* datetime) const
  {
    std::uint8_t buf[7] = { 0 };

    if (!isEnabled() || !readRegister(0x10, buf, 7)) { return false; }

    datetime->time.seconds = bcd2ToByte(buf[0] & 0x7f);
    datetime->time.minutes = bcd2ToByte(buf[1] & 0x7f);
    datetime->time.hours   = bcd2ToByte(buf[2] & 0x3f);
    datetime->date.weekDay = bcd2ToByte((buf[3]+1) & 0x07); // TODO: verify this
    datetime->date.date    = bcd2ToByte(buf[4] & 0x3f);
    datetime->date.month   = bcd2ToByte(buf[5] & 0x1f);
    datetime->date.year    = bcd2ToByte(buf[6] & 0xff) + ((0x80 & buf[5]) ? 1900 : 2000);
    return true;
  }


  void RTC8130_Class::setTime(const rtc_time_t &time)
  {
    std::uint8_t buf[] =
      { byteToBcd2(time.seconds)
      , byteToBcd2(time.minutes)
      , byteToBcd2(time.hours)
      };
    stop(true);
    writeRegister(0x10, buf, sizeof(buf));
    stop(false);
  }



  void RTC8130_Class::setDateTime(const rtc_datetime_t &datetime)
  {
    auto t = datetime.get_tm();
    std::uint8_t buf[] =
    { (std::uint8_t)(byteToBcd2(t.tm_sec) & 0x7F)
    , (std::uint8_t)(byteToBcd2(t.tm_min) & 0x7F)
    , (std::uint8_t)(byteToBcd2(t.tm_hour) & 0x3F)
    , (std::uint8_t)(byteToBcd2(t.tm_wday) & 0x07)
    , (std::uint8_t)(byteToBcd2(t.tm_mday) & 0x3F)
    , (std::uint8_t)(byteToBcd2(t.tm_mon + 1) & 0x1F)
    , (std::uint8_t)(byteToBcd2((t.tm_year + 1900) % 100))
    };

    stop(true);
    writeRegister(0x10, buf, sizeof(buf));
    stop(false);
  }


  void RTC8130_Class::setSystemTimeFromRtc(struct timezone* tz)
  {
    rtc_datetime_t dt;
    if (getDateTime(&dt))
    {
      tm t_st;
      t_st.tm_isdst = -1;
      t_st.tm_year = dt.date.year - 1900;
      t_st.tm_mon  = dt.date.month - 1;
      t_st.tm_mday = dt.date.date;
      t_st.tm_hour = dt.time.hours;
      t_st.tm_min  = dt.time.minutes;
      t_st.tm_sec  = dt.time.seconds;
      timeval now;
      // mktime(3) uses localtime, force UTC
      char *oldtz = getenv("TZ");
      setenv("TZ", "GMT0", 1);
      tzset(); // Workaround for https://github.com/espressif/esp-idf/issues/11455
      now.tv_sec = mktime(&t_st);
      if (oldtz)
      {
        setenv("TZ", oldtz, 1);
      } else {
        unsetenv("TZ");
      }
      now.tv_usec = 0;
      settimeofday(&now, tz);
    } else {
      printf("System dateTime not set, getDateTime() failed\n");
    }
  }



}

