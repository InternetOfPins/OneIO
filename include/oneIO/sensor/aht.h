#pragma once
#include <hapi/hapi.h>
#include <tinyTimeUtils.h>
#include <stdint.h>

namespace oneIO::sensor {

  // AHT10 / AHT20 / AHT30 — I2C temperature + humidity sensor (Aosong/ASAIR).
  //
  // I2C address: 0x38 (fixed, no address pins).
  // Package marking: "ATH1x", "ATH2x", "ATH3x" — all same driver, differ only
  // in the init command byte:
  //   AHT10:  0xE1 (Calibrate | NOR mode)
  //   AHT20:  0xBE (Calibrate | NOR mode) — also AHT30
  //
  // Protocol (single measurement):
  //   1. begin() — sends init, waits 20ms
  //   2. trigger() — sends 0xAC 0x33 0x00, waits 80ms (blocking)
  //   3. read 6 bytes → parse humidity + temperature
  //
  // For repeated measurements in a loop, call measure() which does both steps.
  // Data validity: temperature ±0.3°C, humidity ±2% RH.

  namespace detail {
    static constexpr uint8_t AHT_ADDR       = 0x38;
    static constexpr uint8_t AHT_SOFTRESET  = 0xBA;
    static constexpr uint8_t AHT_TRIGGER    = 0xAC;
    static constexpr uint8_t AHT_STATUS_BUSY = (1<<7);
    static constexpr uint8_t AHT_STATUS_CAL  = (1<<3);
  }

  /// @brief AHT10/20/30 temperature+humidity sensor driver; read() returns {temp_c, rh_pct}
  template<typename TwiMaster,
           uint8_t InitCmd = 0xBE,  // 0xE1 for AHT10, 0xBE for AHT20/AHT30
           uint8_t Addr    = detail::AHT_ADDR>
  struct AHT {
    struct SensorDef { SensorDef() = delete; };

    template<typename O>
    struct Part : O {
      inline static int16_t  _tempC10 = 0;   // temperature × 10 (°C)
      inline static uint16_t _rhPct10 = 0;   // humidity × 10 (%RH)
      inline static bool     _ok      = false;

      static void begin() {
        TwiMaster::begin();
        _reset();
        TinyTimeUtils::ms_delay(20);
        _init();
        TinyTimeUtils::ms_delay(10);
        O::begin();
      }

      // Trigger a measurement and block until data ready (~80ms).
      static bool measure() {
        _trigger();
        TinyTimeUtils::ms_delay(80);
        return _read();
      }

      // Accessors — valid after measure() returns true
      static int16_t  tempC10() { return _tempC10; }  // e.g. 235 = 23.5°C
      static uint16_t rhPct10() { return _rhPct10; }  // e.g. 652 = 65.2%RH
      static bool     valid()   { return _ok; }

    private:
      static void _reset() {
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(detail::AHT_SOFTRESET);
        TwiMaster::end_write();
      }

      static void _init() {
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(InitCmd);
        TwiMaster::write_byte(0x08);
        TwiMaster::write_byte(0x00);
        TwiMaster::end_write();
      }

      static void _trigger() {
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(detail::AHT_TRIGGER);
        TwiMaster::write_byte(0x33);
        TwiMaster::write_byte(0x00);
        TwiMaster::end_write();
      }

      static bool _read() {
        TwiMaster::request_from(Addr, uint8_t(6));
        uint8_t b[6];
        for (auto& v : b) v = TwiMaster::read_byte();

        if (b[0] & detail::AHT_STATUS_BUSY) { _ok = false; return false; }

        // Raw 20-bit fields:
        //   humidity:    b[1]<<12 | b[2]<<4 | b[3]>>4
        //   temperature: (b[3]&0x0F)<<16 | b[4]<<8 | b[5]
        uint32_t rawH = ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) | (b[3] >> 4);
        uint32_t rawT = ((uint32_t)(b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | b[5];

        // humidity %RH × 10 = rawH * 1000 / 2^20
        _rhPct10 = uint16_t((rawH * 1000UL) >> 20);
        // temperature °C × 10 = rawT * 2000 / 2^20 - 500
        _tempC10 = int16_t(((rawT * 2000UL) >> 20) - 500);

        _ok = true;
        return true;
      }
    };

    using Api = hapi::APIOf<SensorDef, AHT<TwiMaster, InitCmd, Addr>>;
  };

} // oneIO::sensor

#ifdef ARDUINO
#include <oneIO/display/arduinoWire.h>
namespace oneIO::sensor {

  // Convenience aliases
  template<TwoWire& wire, uint8_t InitCmd = 0xBE, int sda = -1, int scl = -1>
  struct AHTWire : AHT<oneIO::display::ArduinoWire<wire, sda, scl>, InitCmd> {};

  template<TwoWire& wire, int sda = -1, int scl = -1>
  struct AHT10Wire : AHT<oneIO::display::ArduinoWire<wire, sda, scl>, 0xE1> {};

  template<TwoWire& wire, int sda = -1, int scl = -1>
  struct AHT20Wire : AHT<oneIO::display::ArduinoWire<wire, sda, scl>, 0xBE> {};

  template<TwoWire& wire, int sda = -1, int scl = -1>
  struct AHT30Wire : AHT<oneIO::display::ArduinoWire<wire, sda, scl>, 0xBE> {};

} // oneIO::sensor
#endif
