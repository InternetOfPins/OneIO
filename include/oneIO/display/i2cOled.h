#pragma once
#include <oneIO/display/ssd1306.h>

namespace oneIO::display {

  // I2C transport for SSD1306.
  // TwiMaster must provide begin_write(addr)/write_byte(b)/end_write().
  // Addr: 0x3C (default) or 0x3D depending on SA0 pin.
  template<typename TwiMaster, uint8_t Addr = 0x3C>
  struct I2cSsd1306Transport {
    static void begin() { TwiMaster::begin(); }

    static void cmd(uint8_t c) {
      TwiMaster::begin_write(Addr);
      TwiMaster::write_byte(0x00);  // control: command stream
      TwiMaster::write_byte(c);
      TwiMaster::end_write();
    }

    static void data(const uint8_t* buf, uint8_t len) {
      TwiMaster::begin_write(Addr);
      TwiMaster::write_byte(0x40);  // control: data stream
      while (len--) TwiMaster::write_byte(*buf++);
      TwiMaster::end_write();
    }

    // Chunked fill — 32 bytes per I2C transaction, safe for all TwiMaster
    // implementations including Arduino Wire (32-byte AVR / 128-byte ESP32 buffer).
    static void fill(uint8_t b, uint16_t count) {
      constexpr uint8_t CHUNK = 32;
      while (count > 0) {
        uint8_t n = (count > CHUNK) ? CHUNK : (uint8_t)count;
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(0x40);
        for (uint8_t i = 0; i < n; i++) TwiMaster::write_byte(b);
        TwiMaster::end_write();
        count -= n;
      }
    }
  };

  // Ready-to-use I2C OLED aliases.
  // Width×Height: 128×64 (default) or 128×32.
  template<typename TwiMaster, uint8_t Addr = 0x3C,
           uint8_t Width = 128, uint8_t Height = 64>
  using I2cOled = hapi::APIOf<OledDef,
    Ssd1306<I2cSsd1306Transport<TwiMaster, Addr>, Width, Height>>;

  template<typename TwiMaster, uint8_t Addr = 0x3C>
  using I2cOled32 = I2cOled<TwiMaster, Addr, 128, 32>;

} // oneIO::display
