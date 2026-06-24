#pragma once
#include <stdint.h>
#include <type_traits>

namespace oneIO::display {

  // Generic SPI transport for DC-based TFT displays (ST7735, ILI9341, …).
  // Identical to SpiSsd1306Transport but adds fill16() for RGB565 pixel floods.
  // RstPin=void → skip hardware reset.
  /// @brief SPI transport for DC-pin TFTs; cmd/data/fill/fill16 primitives
  template<typename SpiMaster, typename CsPin, typename DcPin,
           typename RstPin = void>
  struct SpiTftTransport {
    static void begin() {
      CsPin::begin(); DcPin::begin();
      CsPin::on();
      SpiMaster::begin();
      if constexpr (!std::is_same<RstPin, void>::value) {
        RstPin::begin();
        RstPin::off();
        for (volatile uint16_t i = 0; i < 10000; ++i);  // ~10ms
        RstPin::on();
        for (volatile uint16_t i = 0; i < 10000; ++i);  // ~10ms
      }
    }

    static void cmd(uint8_t c) {
      DcPin::off();
      CsPin::off();
      SpiMaster::transfer(&c, 1);
      CsPin::on();
    }

    static void data(const uint8_t* buf, uint16_t len) {
      DcPin::on();
      CsPin::off();
      while (len--) SpiMaster::transfer(*buf++);
      CsPin::on();
    }

    // Send one data byte
    static void data(uint8_t b) {
      DcPin::on();
      CsPin::off();
      SpiMaster::transfer(b);
      CsPin::on();
    }

    // Fill 'count' identical bytes
    static void fill(uint8_t b, uint32_t count) {
      DcPin::on();
      CsPin::off();
      while (count--) SpiMaster::transfer(b);
      CsPin::on();
    }

    // Fill 'count' pixels as big-endian RGB565 words
    static void fill16(uint16_t color, uint32_t count) {
      uint8_t hi = uint8_t(color >> 8);
      uint8_t lo = uint8_t(color);
      DcPin::on();
      CsPin::off();
      while (count--) {
        SpiMaster::transfer(hi);
        SpiMaster::transfer(lo);
      }
      CsPin::on();
    }
  };

} // oneIO::display
