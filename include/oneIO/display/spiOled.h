#pragma once
#include <oneIO/display/ssd1306.h>  // pulls hapi.h → std polyfill on AVR

namespace oneIO::display {

  // SPI transport for SSD1306.
  // SpiMaster: must provide begin(), fill(b, count), transfer(buf, len).
  /// @brief SPI transport for SSD1306; RstPin=void to skip reset
  template<typename SpiMaster, typename CsPin, typename DcPin,
           typename RstPin = void>
  struct SpiSsd1306Transport {
    static void begin() {
      CsPin::begin(); DcPin::begin();
      CsPin::on();  // deassert CS
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
      DcPin::off();   // DC=0 → command
      CsPin::off();
      SpiMaster::transfer(&c, 1);
      CsPin::on();
    }

    static void data(const uint8_t* buf, uint8_t len) {
      DcPin::on();    // DC=1 → data
      CsPin::off();
      SpiMaster::transfer(buf, len);
      CsPin::on();
    }

    static void fill(uint8_t b, uint16_t count) {
      DcPin::on();
      CsPin::off();
      SpiMaster::fill(b, count);
      CsPin::on();
    }
  };

  // Ready-to-use SPI OLED aliases.
  template<typename SpiMaster, typename CsPin, typename DcPin,
           typename RstPin = void, uint8_t Width = 128, uint8_t Height = 64>
  using SpiOled = hapi::APIOf<OledDef,
    Ssd1306<SpiSsd1306Transport<SpiMaster, CsPin, DcPin, RstPin>, Width, Height>>;

} // oneIO::display
