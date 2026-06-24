/**
 * @file ds18b20.h
 * @author Rui Azevedo (ruihfazevedo@gmail.com)
 * @brief DS18B20 1-Wire temperature sensor — HAPI component for OneIO.
 */

#pragma once
#include <hapi/hapi.h>
#include <oneChip/clock.h>
#include <stdint.h>

namespace oneIO::sensor {

  // DS18B20 — Maxim/Dallas 1-Wire temperature sensor, 9–12-bit resolution.
  //
  // OneWireBus must provide: begin(), reset() → bool, skip(), writeByte(u8), readByte() → u8.
  // Compatible with oneBus::OneWire<PinN> (Arduino) or any OneWireMaster<Core> chain.
  //
  // Non-blocking: trigger() → wait 750ms → read()
  // Blocking:     sample() blocks ~750ms internally
  /// @brief DS18B20 1-Wire temperature sensor; sample() returns Reading with tempC()
  template<typename OneWireBus>
  struct DS18B20 {
    struct SensorDef { SensorDef() = delete; };
    using Api = hapi::APIOf<SensorDef, DS18B20<OneWireBus>>;

    struct Reading {
      int16_t raw;    // signed 16-bit, 1/16 °C per LSB
      bool    ok;     // false if no device on bus

      float   tempC()   const { return raw / 16.0f; }
      int16_t tempX16() const { return raw; }
    };

    template<typename O>
    struct Part : O {
      static void begin() { OneWireBus::begin(); O::begin(); }

      static bool trigger() {
        if (!OneWireBus::reset()) return false;
        OneWireBus::skip();
        OneWireBus::writeByte(0x44);  // Convert T
        return true;
      }

      static Reading read() {
        if (!OneWireBus::reset()) return { 0, false };
        OneWireBus::skip();
        OneWireBus::writeByte(0xBE);  // Read Scratchpad
        uint8_t lo = OneWireBus::readByte();
        uint8_t hi = OneWireBus::readByte();
        for (uint8_t i = 2; i < 9; ++i) OneWireBus::readByte();
        return { int16_t(uint16_t(hi) << 8 | lo), true };
      }

      static Reading sample() {
        if (!trigger()) return { 0, false };
        hw::delay_ms(750);
        return read();
      }
    };
  };

} // oneIO::sensor
