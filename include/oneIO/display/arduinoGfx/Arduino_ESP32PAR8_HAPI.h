/*
 * HAPI-composed Arduino_GFX adapter, hosted in OneIO -- see README.md in this
 * directory for scope and rationale (moved here from the Arduino_GFX fork's
 * own src/hapi/, to avoid a real include-path collision with IOP's own
 * HAPI library (both name a top-level hapi/ folder).
 *
 * Compile-time-parameterized stand-in for src/databus/Arduino_ESP32PAR8.{h,cpp}.
 * Same PORT-register read/write strategy (W1TS/W1TC set/clear registers, an
 * 8-bit-value -> pin-mask lookup table), but dc/cs/wr/rd/d0-d7 are non-type
 * template parameters instead of runtime int8_t constructor arguments:
 *
 *  - no Arduino_DataBus base, no vtable: every method below is a concrete,
 *    inlinable call, never an indirect one through a base-class pointer.
 *  - no per-instance _dcPinMask/_csPinMask/_wrPinMask/_data1ClrMask/
 *    _data2ClrMask fields: each is folded to an immediate at the point of
 *    use (verified against real xtensa-esp32-elf-g++ codegen -- see
 *    README.md in this directory, "why not constexpr pointers").
 *  - no per-instance _xset_mask1[256]/_xset_mask2[256] (up to 2KB RAM in the
 *    original, per bus instance): these become `static constexpr`
 *    std::array<uint32_t,256>, i.e. .rodata (flash), computed once at
 *    compile time and shared by every instance of this exact pin
 *    combination -- not recomputed, not RAM-resident.
 *  - `if constexpr` on the cs/rd-present checks removes the CS/RD toggle
 *    code paths entirely at compile time when cs/rd == GFX_NOT_DEFINED,
 *    instead of a runtime branch taken on every single write.
 */
#pragma once

#include <Arduino_DataBus.h>

#if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32C5)

#include <array>

#if (CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32C5)
#define HAPI_ESP32PAR8_HAS_PORT2 1
#else
#define HAPI_ESP32PAR8_HAS_PORT2 0
#endif

namespace hapi_gfx
{

  // Deliberately NOT constexpr: reinterpret_cast from a fixed integer address
  // to a pointer is not a core constant expression in standard C++ (verified
  // against the real toolchain), even though the address is compile-time
  // known. Left as plain always-inline functions, the cast still folds to a
  // literal-pool load of the address at the call site under -O2 -- no stored
  // pointer variable, no indirection, same codegen a constexpr would have
  // produced had the language allowed it. See README.md in this directory.
  static GFX_INLINE PORTreg_t esp32par8_portSet(int8_t pin)
  {
#if HAPI_ESP32PAR8_HAS_PORT2
    return (pin >= 32) ? (PORTreg_t)GPIO_OUT1_W1TS_REG : (PORTreg_t)GPIO_OUT_W1TS_REG;
#else
    return (PORTreg_t)GPIO_OUT_W1TS_REG;
#endif
  }

  static GFX_INLINE PORTreg_t esp32par8_portClr(int8_t pin)
  {
#if HAPI_ESP32PAR8_HAS_PORT2
    return (pin >= 32) ? (PORTreg_t)GPIO_OUT1_W1TC_REG : (PORTreg_t)GPIO_OUT_W1TC_REG;
#else
    return (PORTreg_t)GPIO_OUT_W1TC_REG;
#endif
  }

  // The 8 data-line registers are always these two fixed ports (whichever
  // specific GPIOs d0-d7 land on), never chosen by any one pin's own value --
  // mirrors the original's hardcoded _data1PortSet/_data2PortSet, not a
  // pin-dependent lookup like DC/CS/WR (which really can be on either port).
  static GFX_INLINE PORTreg_t esp32par8_data1PortSet() { return (PORTreg_t)GPIO_OUT_W1TS_REG; }
  static GFX_INLINE PORTreg_t esp32par8_data1PortClr() { return (PORTreg_t)GPIO_OUT_W1TC_REG; }
#if HAPI_ESP32PAR8_HAS_PORT2
  static GFX_INLINE PORTreg_t esp32par8_data2PortSet() { return (PORTreg_t)GPIO_OUT1_W1TS_REG; }
  static GFX_INLINE PORTreg_t esp32par8_data2PortClr() { return (PORTreg_t)GPIO_OUT1_W1TC_REG; }
#endif

  template <int8_t dc, int8_t cs, int8_t wr, int8_t rd,
            int8_t d0, int8_t d1, int8_t d2, int8_t d3, int8_t d4, int8_t d5, int8_t d6, int8_t d7>
  class Arduino_ESP32PAR8_HAPI
  {
  public:
    bool begin(int32_t = GFX_NOT_DEFINED, int8_t = GFX_NOT_DEFINED)
    {
      pinMode(dc, OUTPUT);
      digitalWrite(dc, HIGH); // Data mode

      if constexpr (cs != GFX_NOT_DEFINED)
      {
        pinMode(cs, OUTPUT);
        digitalWrite(cs, HIGH); // disable chip select
      }

      pinMode(wr, OUTPUT);
      digitalWrite(wr, HIGH); // Set write strobe high (inactive)

      if constexpr (rd != GFX_NOT_DEFINED)
      {
        pinMode(rd, OUTPUT);
        digitalWrite(rd, HIGH);
      }

      pinMode(d0, OUTPUT);
      pinMode(d1, OUTPUT);
      pinMode(d2, OUTPUT);
      pinMode(d3, OUTPUT);
      pinMode(d4, OUTPUT);
      pinMode(d5, OUTPUT);
      pinMode(d6, OUTPUT);
      pinMode(d7, OUTPUT);

      *esp32par8_data1PortClr() = _data1ClrMask;
#if HAPI_ESP32PAR8_HAS_PORT2
      *esp32par8_data2PortClr() = _data2ClrMask;
#endif

      return true;
    }

    GFX_INLINE void beginWrite()
    {
      DC_HIGH();
      CS_LOW();
    }

    GFX_INLINE void endWrite()
    {
      CS_HIGH();
    }

    GFX_INLINE void writeCommand(uint8_t c)
    {
      DC_LOW();
      WRITE(c);
      DC_HIGH();
    }

    GFX_INLINE void writeCommand16(uint16_t c)
    {
      DC_LOW();
      WRITE(uint8_t(c >> 8));
      WRITE(uint8_t(c));
      DC_HIGH();
    }

    GFX_INLINE void writeCommandBytes(uint8_t *data, uint32_t len)
    {
      DC_LOW();
      while (len--)
      {
        WRITE(*data++);
      }
      DC_HIGH();
    }

    GFX_INLINE void write(uint8_t d)
    {
      WRITE(d);
    }

    GFX_INLINE void write16(uint16_t d)
    {
      WRITE(uint8_t(d >> 8));
      WRITE(uint8_t(d));
    }

    GFX_INLINE void writeRepeat(uint16_t p, uint32_t len)
    {
      uint8_t msb = uint8_t(p >> 8), lsb = uint8_t(p);
      if (msb == lsb)
      {
        setDataBits(msb);
        while (len--)
        {
          STROBE_WR();
          STROBE_WR();
        }
      }
      else
      {
        while (len--)
        {
          WRITE(msb);
          WRITE(lsb);
        }
      }
    }

    GFX_INLINE void writeBytes(uint8_t *data, uint32_t len)
    {
      while (len--)
      {
        WRITE(*data++);
      }
    }

    GFX_INLINE void writePixels(uint16_t *data, uint32_t len)
    {
      while (len--)
      {
        uint16_t v = *data++;
        WRITE(uint8_t(v >> 8));
        WRITE(uint8_t(v));
      }
    }

    GFX_INLINE void writeC8D8(uint8_t c, uint8_t d)
    {
      DC_LOW();
      WRITE(c);
      DC_HIGH();
      WRITE(d);
    }

    GFX_INLINE void writeC8D16(uint8_t c, uint16_t d)
    {
      DC_LOW();
      WRITE(c);
      DC_HIGH();
      WRITE(uint8_t(d >> 8));
      WRITE(uint8_t(d));
    }

    GFX_INLINE void writeC8D16D16(uint8_t c, uint16_t v1, uint16_t v2)
    {
      DC_LOW();
      WRITE(c);
      DC_HIGH();
      WRITE(uint8_t(v1 >> 8));
      WRITE(uint8_t(v1));
      WRITE(uint8_t(v2 >> 8));
      WRITE(uint8_t(v2));
    }

    GFX_INLINE void writeIndexedPixels(uint8_t *data, uint16_t *idx, uint32_t len)
    {
      while (len--)
      {
        uint16_t v = idx[*data++];
        WRITE(uint8_t(v >> 8));
        WRITE(uint8_t(v));
      }
    }

  private:
    // pin -> bitmask on port 1 (GPIO 0-31); 0 if this pin lives on port 2.
    // constexpr (unlike the port-address helpers above): plain integer
    // arithmetic, no reinterpret_cast, so this one is actually legal in a
    // constant expression -- needed since it feeds the static constexpr
    // _data1ClrMask/_xset_mask1 below.
    static constexpr uint32_t bit1(int8_t pin)
    {
#if HAPI_ESP32PAR8_HAS_PORT2
      return (pin >= 32) ? 0u : (uint32_t)digitalPinToBitMask(pin);
#else
      return (uint32_t)digitalPinToBitMask(pin);
#endif
    }

#if HAPI_ESP32PAR8_HAS_PORT2
    // pin -> bitmask on port 2 (GPIO 32-39); 0 if this pin lives on port 1
    static constexpr uint32_t bit2(int8_t pin)
    {
      return (pin >= 32) ? (uint32_t)digitalPinToBitMask(pin) : 0u;
    }
#endif

    static constexpr uint32_t _data1ClrMask =
        bit1(d0) | bit1(d1) | bit1(d2) | bit1(d3) | bit1(d4) | bit1(d5) | bit1(d6) | bit1(d7);
#if HAPI_ESP32PAR8_HAS_PORT2
    static constexpr uint32_t _data2ClrMask =
        bit2(d0) | bit2(d1) | bit2(d2) | bit2(d3) | bit2(d4) | bit2(d5) | bit2(d6) | bit2(d7);
#endif

    static constexpr std::array<uint32_t, 256> buildXset1()
    {
      std::array<uint32_t, 256> t{};
      for (int c = 0; c < 256; ++c)
      {
        uint32_t m = 0;
        if (c & 0x01)
          m |= bit1(d0);
        if (c & 0x02)
          m |= bit1(d1);
        if (c & 0x04)
          m |= bit1(d2);
        if (c & 0x08)
          m |= bit1(d3);
        if (c & 0x10)
          m |= bit1(d4);
        if (c & 0x20)
          m |= bit1(d5);
        if (c & 0x40)
          m |= bit1(d6);
        if (c & 0x80)
          m |= bit1(d7);
        t[c] = m;
      }
      return t;
    }
    static constexpr std::array<uint32_t, 256> _xset_mask1 = buildXset1();

#if HAPI_ESP32PAR8_HAS_PORT2
    static constexpr std::array<uint32_t, 256> buildXset2()
    {
      std::array<uint32_t, 256> t{};
      for (int c = 0; c < 256; ++c)
      {
        uint32_t m = 0;
        if (c & 0x01)
          m |= bit2(d0);
        if (c & 0x02)
          m |= bit2(d1);
        if (c & 0x04)
          m |= bit2(d2);
        if (c & 0x08)
          m |= bit2(d3);
        if (c & 0x10)
          m |= bit2(d4);
        if (c & 0x20)
          m |= bit2(d5);
        if (c & 0x40)
          m |= bit2(d6);
        if (c & 0x80)
          m |= bit2(d7);
        t[c] = m;
      }
      return t;
    }
    static constexpr std::array<uint32_t, 256> _xset_mask2 = buildXset2();
#endif

    static GFX_INLINE void setDataBits(uint8_t d)
    {
      uint32_t setMask1 = _xset_mask1[d];
      *esp32par8_data1PortClr() = _data1ClrMask;
#if HAPI_ESP32PAR8_HAS_PORT2
      uint32_t setMask2 = _xset_mask2[d];
      *esp32par8_data2PortClr() = _data2ClrMask;
#endif
      *esp32par8_data1PortSet() = setMask1;
#if HAPI_ESP32PAR8_HAS_PORT2
      *esp32par8_data2PortSet() = setMask2;
#endif
    }

    static GFX_INLINE void STROBE_WR()
    {
      *esp32par8_portClr(wr) = _wrPinMask;
      *esp32par8_portSet(wr) = _wrPinMask;
    }

    static GFX_INLINE void WRITE(uint8_t d)
    {
      setDataBits(d);
      STROBE_WR();
    }

    static constexpr uint32_t _dcPinMask = digitalPinToBitMask(dc);
    static constexpr uint32_t _wrPinMask = digitalPinToBitMask(wr);
    static constexpr uint32_t _csPinMask = (cs != GFX_NOT_DEFINED) ? (uint32_t)digitalPinToBitMask(cs) : 0u;

    static GFX_INLINE void DC_HIGH() { *esp32par8_portSet(dc) = _dcPinMask; }
    static GFX_INLINE void DC_LOW() { *esp32par8_portClr(dc) = _dcPinMask; }
    static GFX_INLINE void CS_HIGH()
    {
      if constexpr (cs != GFX_NOT_DEFINED)
        *esp32par8_portSet(cs) = _csPinMask;
    }
    static GFX_INLINE void CS_LOW()
    {
      if constexpr (cs != GFX_NOT_DEFINED)
        *esp32par8_portClr(cs) = _csPinMask;
    }
  };

} // namespace hapi_gfx

#endif // #if defined(ESP32) && (...)
