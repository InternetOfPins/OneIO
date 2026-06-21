#pragma once
#include <hapi/hapi.h>
#ifdef ARDUINO
#include <Arduino.h>

namespace oneIO::rf {

  // Protocol timing — pulse-length multiples: {syncHi, syncLo, b0Hi, b0Lo, b1Hi, b1Lo}
  template<uint8_t SyncHi, uint8_t SyncLo,
           uint8_t B0Hi,   uint8_t B0Lo,
           uint8_t B1Hi,   uint8_t B1Lo>
  struct RfProto {
    static constexpr uint8_t syncHi = SyncHi, syncLo = SyncLo;
    static constexpr uint8_t b0Hi   = B0Hi,   b0Lo   = B0Lo;
    static constexpr uint8_t b1Hi   = B1Hi,   b1Lo   = B1Lo;
  };

  using Proto1 = RfProto<1, 31,  1,  3,  3,  1>;
  using Proto2 = RfProto<1, 10,  1,  2,  2,  1>;
  using Proto3 = RfProto<1, 71,  4, 11,  9,  6>;

  // TinyRC<TxPin, RxPin, Proto, PulseLen, RecvBits>
  //
  // TxPin: IOP OutPin type — on()/off()/begin()
  // RxPin: IOP InPin type  — get() returns current level (called from ISR)
  //
  // Wire the ISR in your platform file:
  //   attachInterrupt(digitalPinToInterrupt(RX_PIN), MyRC::handleInterrupt, CHANGE);
  template<typename TxPin, typename RxPin, typename Proto,
           uint16_t PulseLen, uint8_t RecvBits = 24>
  struct TinyRC {

    // ── TX ────────────────────────────────────────────────────────────────

    inline static volatile bool _transmitting = false;

    static void begin() { TxPin::begin(); RxPin::begin(); }

    static void pulse(uint8_t hi, uint8_t lo) {
      TxPin::on();  delayMicroseconds(uint32_t(PulseLen) * hi);
      TxPin::off(); delayMicroseconds(uint32_t(PulseLen) * lo);
    }

    static void send0()    { pulse(Proto::b0Hi,   Proto::b0Lo); }
    static void send1()    { pulse(Proto::b1Hi,   Proto::b1Lo); }
    static void sendSync() { pulse(Proto::syncHi, Proto::syncLo); }

    static void sendT0()   { pulse(1,3); pulse(1,3); }
    static void sendT1()   { pulse(3,1); pulse(3,1); }
    static void sendTF()   { pulse(1,3); pulse(3,1); }

    static void sendCode(uint32_t code, uint8_t bits = RecvBits, uint8_t repeat = 10) {
      _transmitting = true;
      for (uint8_t r = 0; r < repeat; r++) {
        sendSync();
        for (int8_t i = bits - 1; i >= 0; i--)
          (code >> i) & 1u ? send1() : send0();
      }
      _transmitting = false;
    }

    // ── RX ────────────────────────────────────────────────────────────────

    inline static volatile uint32_t _lastTime  = 0;
    inline static volatile uint32_t _temp      = 0;
    inline static volatile uint32_t _received  = 0;
    inline static volatile uint8_t  _bitCount  = 0;
    inline static volatile bool     _synced    = false;
    inline static volatile bool     _available = false;

    // Bit-high threshold: midpoint between b0Hi and b1Hi pulse lengths
    static constexpr uint32_t _hiThresh =
      uint32_t(PulseLen) * (Proto::b0Hi + Proto::b1Hi) / 2;

    // Sync: LOW pulse ≈ syncLo × PulseLen (±40%)
    static constexpr uint32_t _syncMin =
      uint32_t(PulseLen) * Proto::syncLo * 6 / 10;
    static constexpr uint32_t _syncMax =
      uint32_t(PulseLen) * Proto::syncLo * 14 / 10;

    // Call from ISR (CHANGE) — decode pulse widths into bits.
    // Sync detected on long LOW pulse; bits decoded from subsequent HIGH pulses.
    static void handleInterrupt() {
      if (_transmitting) return;

      const uint32_t now      = micros();
      const uint32_t duration = now - _lastTime;
      _lastTime = now;

      if (RxPin::get()) {
        // just went HIGH — duration was the LOW pulse
        if (duration >= _syncMin && duration <= _syncMax) {
          _synced   = true;
          _bitCount = 0;
          _temp     = 0;
        }
      } else {
        // just went LOW — duration was the HIGH pulse → one data bit
        if (_synced) {
          _temp = (_temp << 1) | (duration > _hiThresh ? 1u : 0u);
          if (++_bitCount == RecvBits) {
            _received  = _temp;
            _available = true;
            _synced    = false;
          }
        }
      }
    }

    static bool     available() { return _available; }
    static uint32_t read()      { _available = false; return _received; }
  };

} // oneIO::rf
#endif
