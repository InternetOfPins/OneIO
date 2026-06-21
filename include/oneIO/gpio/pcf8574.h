#pragma once
#include <hapi/hapi.h>
#include <stdint.h>
#ifdef ARDUINO
#include <oneIO/display/arduinoWire.h>

namespace oneIO::gpio {

  // PCF8574 / PCF8574T / PCF8574A — I2C 8-bit quasi-bidirectional GPIO expander.
  //
  // All 8 pins are open-drain with weak pull-ups.
  // To use a pin as input: write 1 (releases the line); then read the byte.
  // To drive a pin LOW (output): write 0.
  // To drive a pin HIGH (output): write 1 (relies on pull-up — not a strong drive).
  //
  // Default I2C addresses:
  //   PCF8574  (Texas Instruments): 0x20..0x27  (A2 A1 A0 + base 0x20)
  //   PCF8574T (NXP):               0x20..0x27  (same)
  //   PCF8574A / PCF8574AT:         0x38..0x3F
  //
  // TwiMaster: ArduinoWire<wire> or compatible.
  template<typename TwiMaster, uint8_t Addr = 0x27>
  struct PCF8574 {
    struct GpioDef { GpioDef() = delete; };

    template<typename O>
    struct Part : O {
      inline static uint8_t _latch = 0xFF;  // output latch, default all HIGH (inputs)

      static void begin() { TwiMaster::begin(); O::begin(); }

      // Write all 8 pins at once
      static void write(uint8_t val) {
        _latch = val;
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(val);
        TwiMaster::end_write();
      }

      // Read all 8 pins (must have written 1s to input pins first)
      static uint8_t read() {
        TwiMaster::request_from(Addr, uint8_t(1));
        return TwiMaster::read_byte();
      }

      // Set individual pins HIGH (release/input) without disturbing others
      static void set(uint8_t mask)   { write(_latch |  mask); }
      // Drive individual pins LOW without disturbing others
      static void clr(uint8_t mask)   { write(_latch & ~mask); }
      // Toggle individual pins
      static void toggle(uint8_t mask){ write(_latch ^  mask); }
      // Read after releasing pins to input mode
      static uint8_t get(uint8_t mask) {
        set(mask);           // release pins → input mode
        return read() & mask;
      }

      static uint8_t latch() { return _latch; }
    };

    using Api = hapi::APIOf<GpioDef, PCF8574<TwiMaster, Addr>>;
  };

  // Convenience alias for Arduino Wire
  template<TwoWire& wire, uint8_t Addr = 0x27, int sda = -1, int scl = -1>
  struct PCF8574Wire : PCF8574<oneIO::display::ArduinoWire<wire, sda, scl>, Addr> {};

} // oneIO::gpio
#endif
