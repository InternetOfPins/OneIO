#pragma once
#include <hapi/hapi.h>
#include <stdint.h>

namespace oneIO::rtc {

  /// @brief RTC date/time value: year/month/day/hour/min/sec + dow (1=Sun..7=Sat)
  struct DateTime {
    uint16_t year;
    uint8_t  month, day;
    uint8_t  hour, min, sec;
    uint8_t  dow;   // 1=Sunday .. 7=Saturday (DS3231 register convention)
  };

  struct RtcDef {
    RtcDef() = delete;
    static void begin() {}
  };

  // DS3231 I2C RTC + temperature sensor driver.
  // TwiMaster must provide:
  //   begin(), begin_write(addr), write_byte(b), end_write(),
  //   request_from(addr, n), read_byte()
  // Compatible with oneIO::display::ArduinoWire<wire> (extended with read ops).
  /// @brief DS3231 I2C RTC + temperature sensor; now() reads DateTime, set() writes, tempC() in °C
  template<typename TwiMaster, uint8_t Addr = 0x68>
  struct Ds3231 {
    template<typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;

    private:
      static uint8_t bcd2bin(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
      static uint8_t bin2bcd(uint8_t b) { return ((b / 10) << 4) | (b % 10); }

      static void readBurst(uint8_t reg, uint8_t* buf, uint8_t len) {
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(reg);
        TwiMaster::end_write();
        TwiMaster::request_from(Addr, len);
        for (uint8_t i = 0; i < len; i++) buf[i] = TwiMaster::read_byte();
      }

    public:
      static void begin() { TwiMaster::begin(); Base::begin(); }

      static DateTime now() {
        uint8_t buf[7];
        readBurst(0x00, buf, 7);
        return {
          uint16_t(2000u + bcd2bin(buf[6])),
          bcd2bin(buf[5] & 0x1F),
          bcd2bin(buf[4]),
          bcd2bin(buf[2] & 0x3F),
          bcd2bin(buf[1]),
          bcd2bin(buf[0] & 0x7F),
          buf[3]
        };
      }

      static void set(const DateTime& dt) {
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(0x00);
        TwiMaster::write_byte(bin2bcd(dt.sec));
        TwiMaster::write_byte(bin2bcd(dt.min));
        TwiMaster::write_byte(bin2bcd(dt.hour));
        TwiMaster::write_byte(dt.dow);
        TwiMaster::write_byte(bin2bcd(dt.day));
        TwiMaster::write_byte(bin2bcd(dt.month));
        TwiMaster::write_byte(bin2bcd(uint8_t(dt.year - 2000u)));
        TwiMaster::end_write();
      }

      // Temperature in 0.25 °C steps (signed int16)
      static int16_t tempRaw() {
        uint8_t buf[2];
        readBurst(0x11, buf, 2);
        return (int16_t(int8_t(buf[0])) << 2) | (buf[1] >> 6);
      }

      static float tempC() { return tempRaw() * 0.25f; }
    };
  };

  template<typename TwiMaster, uint8_t Addr = 0x68>
  using Ds3231Device = hapi::APIOf<RtcDef, Ds3231<TwiMaster, Addr>>;

} // oneIO::rtc
