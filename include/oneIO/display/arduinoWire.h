#pragma once
#ifdef ARDUINO
#include <Wire.h>

// TwiMaster adapter for Arduino Wire library.
// Use ArduinoWire<Wire, sda, scl> on ESP32 or ArduinoWire<Wire> on AVR.
template<TwoWire& wire, int sda = -1, int scl = -1>
struct ArduinoWire {
  static void begin() {
    if constexpr(sda >= 0) wire.begin(sda, scl);
    else                   wire.begin();
  }
  static void begin_write(uint8_t addr) { wire.beginTransmission(addr); }
  static void write_byte(uint8_t b)     { wire.write(b); }
  static void end_write()               { wire.endTransmission(); }
};
#endif
