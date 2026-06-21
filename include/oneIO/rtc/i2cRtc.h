#pragma once
#include <oneIO/rtc/ds3231.h>
#ifdef ARDUINO
#include <oneIO/display/i2cOled.h>   // ArduinoWire<wire, sda, scl> — now includes read ops

namespace oneIO::rtc {

  // Convenience alias: DS3231 over Arduino Wire.
  // Usage (AVR/Uno):
  //   using MyRtc = Ds3231Wire<Wire>;
  //   MyRtc::begin();
  //   DateTime dt = MyRtc::now();
  //   MyRtc::set({2025, 6, 21, 10, 30, 0, 7});
  //   float t = MyRtc::tempC();
  template<TwoWire& wire, int sda = -1, int scl = -1, uint8_t Addr = 0x68>
  using Ds3231Wire = Ds3231Device<oneIO::display::ArduinoWire<wire, sda, scl>, Addr>;

} // oneIO::rtc
#endif
