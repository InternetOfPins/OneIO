/*
 * HAPI-composed Arduino_GFX adapter, hosted in OneIO -- see README.md in this
 * directory for scope and rationale (moved here from the Arduino_GFX fork's
 * own src/hapi/, which no longer exists, to avoid a real include-path collision
 * with IOP's own HAPI library -- both used to publish a top-level hapi/ folder).
 *
 * Bus bridge for Arduino_ST7789_HAPI<Bus> (etc.) backed by IOP's own,
 * already-existing SPI/pin primitives -- hw::esp32::Esp32SpiMaster (wraps
 * the Arduino core's own SPI class) and hw::esp32::Esp32OutPin (wraps
 * pinMode/digitalWrite) from OneChip's chips/esp32/{esp32Spi,esp32Gpio}.h.
 *
 * Deliberately NOT a port of Arduino_GFX's own Arduino_ESP32SPI (a ~1200-line
 * driver that hand-manipulates the ESP32 hardware SPI peripheral's raw
 * registers, differently per ESP32/S2/S3/C3/C6/P4 variant, plus APB-clock
 * change callbacks and DMA-capable buffer allocation -- verified directly:
 * even the newest espressif32 platform version available in this environment
 * (7.0.1, Arduino-ESP32 core snapshot 3.20017) is missing
 * esp32-hal-periman.h, a header that class unconditionally requires, so the
 * *original* class doesn't even compile here). This is the other side of the
 * comparison this adapter set is making: not "port Arduino_GFX's own SPI driver to
 * be HAPI-shaped," but "what does driving this display look like using
 * IOP/HAPI's own existing composition primitives" -- which, on ESP32 today,
 * means Esp32SpiMaster/Esp32OutPin, not a hand-rolled register-level driver.
 * That means real, honest differences from Arduino_GFX's own bus classes:
 * Esp32OutPin::set() calls Arduino's digitalWrite() (a runtime pin lookup,
 * not a compile-time-folded register address like this adapter set's other HAPI
 * buses), and Esp32SpiMaster::transfer() goes through the Arduino core's own
 * SPIClass, not a custom low-level driver. Reported as-is, not normalized
 * against the original's approach -- see bench/RESULTS.md.
 */
#pragma once

#include <Arduino_DataBus.h>
#include <chips/esp32/esp32Gpio.h>
#include <chips/esp32/esp32Spi.h>

namespace hapi_gfx
{

  template <int Dc, int Cs = GFX_NOT_DEFINED, int Mosi = 23, int Miso = 19, int Sck = 18>
  class Arduino_IOPSPI_HAPI
  {
    using DcPin = hw::esp32::Esp32OutPin<Dc>;
    using CsPin = hw::esp32::Esp32OutPin<Cs>;
    using Spi = hw::esp32::Esp32SpiMaster<Mosi, Miso, Sck>;

  public:
    bool begin(int32_t = GFX_NOT_DEFINED, int8_t = GFX_NOT_DEFINED)
    {
      DcPin::begin();
      DcPin::set(true);
      if constexpr (Cs != GFX_NOT_DEFINED)
      {
        CsPin::begin();
        CsPin::set(true);
      }
      Spi::begin();
      return true;
    }

    void beginWrite()
    {
      DcPin::set(true);
      if constexpr (Cs != GFX_NOT_DEFINED)
        CsPin::set(false);
    }

    void endWrite()
    {
      if constexpr (Cs != GFX_NOT_DEFINED)
        CsPin::set(true);
    }

    void write(uint8_t d) { Spi::transfer(d); }

    void writeCommand(uint8_t c)
    {
      DcPin::set(false);
      Spi::transfer(c);
      DcPin::set(true);
    }

    void writeCommand16(uint16_t c)
    {
      DcPin::set(false);
      Spi::transfer(uint8_t(c >> 8));
      Spi::transfer(uint8_t(c));
      DcPin::set(true);
    }

    void writeCommandBytes(uint8_t *data, uint32_t len)
    {
      DcPin::set(false);
      Spi::transfer(data, len);
      DcPin::set(true);
    }

    void write16(uint16_t d)
    {
      Spi::transfer(uint8_t(d >> 8));
      Spi::transfer(uint8_t(d));
    }

    void writeRepeat(uint16_t p, uint32_t len)
    {
      uint8_t msb = uint8_t(p >> 8), lsb = uint8_t(p);
      while (len--)
      {
        Spi::transfer(msb);
        Spi::transfer(lsb);
      }
    }

    void writeBytes(uint8_t *data, uint32_t len) { Spi::transfer(data, len); }

    void writePixels(uint16_t *data, uint32_t len)
    {
      while (len--)
      {
        uint16_t v = *data++;
        Spi::transfer(uint8_t(v >> 8));
        Spi::transfer(uint8_t(v));
      }
    }

    // Arduino_DataBus provides these as non-virtual defaults on the base
    // class; with no base class here, they're spelled out directly instead
    // (same as every other bus in this adapter set).
    void writeC8D8(uint8_t c, uint8_t d) { writeCommand(c); write(d); }
    void writeC16D16(uint16_t c, uint16_t d) { writeCommand16(c); write16(d); }
    void writeC8D16(uint8_t c, uint16_t d) { writeCommand(c); write16(d); }
    void writeC8D16D16(uint8_t c, uint16_t d1, uint16_t d2) { writeCommand(c); write16(d1); write16(d2); }
    void writeC8D16D16Split(uint8_t c, uint16_t d1, uint16_t d2) { writeC8D16D16(c, d1, d2); }
  };

} // namespace hapi_gfx
