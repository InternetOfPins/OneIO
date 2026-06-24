#pragma once
#include <oneIO/display/ssd1306.h>
#ifdef ARDUINO
#include <Wire.h>
#endif

namespace oneIO::display {

#ifdef ARDUINO
  // TwiMaster adapter for Arduino Wire library.
  // ArduinoWire<Wire, sda, scl> on ESP32/RP2040 (pin-configurable Wire::begin);
  // ArduinoWire<Wire> on AVR (default pins, no pin-config overload).
  /// @brief Arduino Wire I2C adapter; auto-detects begin(sda,scl) vs begin() via SFINAE
  template<TwoWire& wire, int sda = -1, int scl = -1>
  struct ArduinoWire {
    static void    begin()                       { _begin(wire); }
    static void    begin_write(uint8_t addr)     { wire.beginTransmission(addr); }
    static void    write_byte(uint8_t b)         { wire.write(b); }
    static void    end_write()                   { wire.endTransmission(); }
    static uint8_t request_from(uint8_t addr, uint8_t n) {
      return (uint8_t)wire.requestFrom(addr, (uint8_t)n);
    }
    static uint8_t read_byte()                   { return (uint8_t)wire.read(); }
  private:
    // Detect Wire::begin(int,int) — exists on ESP32/RP2040, not on AVR.
    template<typename W, typename = void>
    struct _HasPinBegin : std::false_type {};
    template<typename W>
    struct _HasPinBegin<W, std::void_t<decltype(std::declval<W&>().begin(0, 0))>>
      : std::true_type {};

    // W is a dependent type so if constexpr can gate the call correctly.
    template<typename W>
    static void _begin(W& w) {
      if constexpr(sda >= 0 && _HasPinBegin<W>::value)
        w.begin(sda, scl);
      else
        w.begin();
    }
  };
#endif

  /// @brief I2C transport for SSD1306; Addr 0x3C (default) or 0x3D depending on SA0 pin
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

#ifdef ARDUINO
  // Convenience aliases for the common Arduino Wire case.
  // I2cOledWire<Wire, sda, scl> — single include, no separate ArduinoWire<> typedef needed.
  template<TwoWire& wire, int sda = -1, int scl = -1,
           uint8_t Addr = 0x3C, uint8_t Width = 128, uint8_t Height = 64>
  using I2cOledWire = I2cOled<ArduinoWire<wire, sda, scl>, Addr, Width, Height>;

  template<TwoWire& wire, int sda = -1, int scl = -1, uint8_t Addr = 0x3C>
  using I2cOledWire32 = I2cOledWire<wire, sda, scl, Addr, 128, 32>;
#endif

} // oneIO::display
