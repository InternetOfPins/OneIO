#pragma once
#include <hapi/hapi.h>
#ifdef ARDUINO
#include <Arduino.h>

namespace oneIO::rf {

  // OneSw 433MHz packet format — 32-bit, MSB first
  union SwData {
    uint32_t raw;
    uint8_t  bytes[4];
    struct {
      uint8_t  tag;
      struct { uint8_t cmd:7; uint8_t mut:1; };
      uint16_t id;
    };
  };

  // OneSwRcv<RxPin, onData, CrcFn, Sync, Bit, One, Space>
  //
  // RxPin : IOP InPin type — get() returns current level; begin() sets INPUT_PULLUP
  // onData: callback invoked with valid packet
  // CrcFn : optional CRC check — nullptr skips validation
  //
  // Protocol (all µs):
  //   LOW ≥ Sync  → packet sync (reset)
  //   LOW ≥ Bit   → bit pulse; period since last bit ≥ One = 1, else 0
  //   period ≥ Space → gap too long, discard
  //
  // Wire ISR in platform file (fixes original hardcoded interrupt 0):
  //   attachInterrupt(digitalPinToInterrupt(PIN), MyRcv::handleInterrupt, CHANGE);
  template<typename RxPin, void(*onData)(SwData),
           bool(*CrcFn)(uint32_t) = nullptr,
           long Sync = 8000, long Bit = 400, long One = 2000, long Space = 32000>
  struct OneSwRcv {

    inline static volatile SwData   _data {0};
    inline static volatile uint8_t  _cnt  = 0;
    inline static volatile uint32_t _on   = 0;    // micros() when pin went LOW
    inline static volatile uint32_t _last = 0;    // micros() of last valid bit

    static void begin() { RxPin::begin(); }

    static void handleInterrupt() {
      const uint32_t now   = micros();
      const bool     state = RxPin::get();

      if (state) {
        // pin just went HIGH — radio off
        // len = duration of LOW (radio-on) pulse
        const long len = (long)(now - _on);
        if (len >= Sync) {
          _data.raw = 0;
          _cnt      = 0;
        } else if (len >= Bit) {
          // valid bit: period since last bit determines 0 or 1
          const long period = (long)(now - _last);
          _last = now;
          if (period >= Space) return;
          _data.raw <<= 1;
          if (period >= One) _data.raw ^= 1u;
          if (++_cnt >= 32) {
            if (!CrcFn || CrcFn(_data.raw)) onData(_data);
            _data.raw = 0;
            _cnt      = 0;
          }
        }
      } else {
        // pin just went LOW — radio on
        _on = now;
      }
    }
  };

} // oneIO::rf
#endif
