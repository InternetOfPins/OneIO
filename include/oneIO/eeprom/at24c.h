#pragma once
#include <hapi/hapi.h>
#include <stdint.h>
#ifdef ARDUINO
#include <Arduino.h>
#include <oneIO/display/i2cOled.h>   // ArduinoWire<wire, sda, scl>

namespace oneIO::eeprom {

  // AT24Cxx I2C EEPROM driver — raw byte read/write.
  // TwiMaster: ArduinoWire<wire> or any type providing
  //   begin(), begin_write(addr), write_byte(b), end_write(),
  //   request_from(addr, n), read_byte()
  // Addr:     0x50..0x57 (A0-A2 pins)
  // PageSize: 32 bytes (AT24C32/64/128/256) or 8 (AT24C01/02/04/08/16)
  template<typename TwiMaster, uint8_t Addr = 0x50, uint8_t PageSize = 32>
  struct AT24C {
    static void begin() { TwiMaster::begin(); }

    // Sequential read — chunked to 32 bytes per request (AVR Wire buffer limit)
    static void read(uint16_t addr, uint8_t* buf, uint16_t len) {
      constexpr uint8_t CHUNK = 32;
      while (len > 0) {
        uint8_t n = (len > CHUNK) ? CHUNK : uint8_t(len);
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(uint8_t(addr >> 8));
        TwiMaster::write_byte(uint8_t(addr));
        TwiMaster::end_write();
        TwiMaster::request_from(Addr, n);
        for (uint8_t i = 0; i < n; i++) *buf++ = TwiMaster::read_byte();
        addr += n; len -= n;
      }
    }

    // Page-aligned write — respects page boundary, 5 ms write cycle per page
    static void write(uint16_t addr, const uint8_t* buf, uint16_t len) {
      while (len > 0) {
        uint8_t pageOff = uint8_t(addr & (PageSize - 1));
        uint8_t n = PageSize - pageOff;
        if (uint16_t(n) > len) n = uint8_t(len);
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(uint8_t(addr >> 8));
        TwiMaster::write_byte(uint8_t(addr));
        for (uint8_t i = 0; i < n; i++) TwiMaster::write_byte(*buf++);
        TwiMaster::end_write();
        delay(5);
        addr += n; len -= n;
      }
    }
  };

  // Convenience alias for Arduino Wire
  template<TwoWire& wire, int sda = -1, int scl = -1,
           uint8_t Addr = 0x50, uint8_t PageSize = 32>
  using AT24CWire = AT24C<oneIO::display::ArduinoWire<wire, sda, scl>, Addr, PageSize>;

} // oneIO::eeprom
#endif
