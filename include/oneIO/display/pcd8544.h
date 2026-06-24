#pragma once
#include <hapi/hapi.h>
#include <stdint.h>
#include <oneIO/display/font5x8.h>

// Nokia 5110 / PCD8544 — 84×48 monochrome LCD, SPI.
//
// Same page-based API contract as Ssd1306:
//   setCursor(col_px, page), print(char/str), fillRect(), setInverted(), draw_col()
// Drops directly into oledOut.h / GfxFmt without changes.
//
// Wiring (5 signal pins + VCC + GND):
//   RST  — reset, active low
//   CE   — chip enable (CS), active low
//   D/C  — data/command select (0=cmd, 1=data)
//   DIN  — SPI MOSI
//   CLK  — SPI clock
//   VCC  — 3.3 V (use level shifter or 3.3V-tolerant board on 5V systems)
//   LED  — backlight anode (via ~330Ω resistor to VCC)
//
// Contrast: Vop register (extended instruction set), 7-bit value 0x00-0x7F.
// Typical range 0x30-0x50; default 0x3F gives full contrast.
// Adjust per physical display — they vary significantly between batches.

namespace oneIO::display {

  struct LcdDef {
    LcdDef() = delete;
    static void begin() {}
  };

  /// @brief SPI transport for Nokia 5110 PCD8544 LCD; RstPin=void to skip reset
  template<typename SpiMaster, typename CsPin, typename DcPin,
           typename RstPin = void>
  struct SpiPcd8544Transport {
    static void begin() {
      CsPin::begin(); DcPin::begin();
      CsPin::on();    // deassert CS
      SpiMaster::begin();
      if constexpr (!std::is_same_v<RstPin, void>) {
        RstPin::begin();
        RstPin::off();                                 // reset active low
        for (volatile uint16_t i = 0; i < 10000; ++i);
        RstPin::on();
        for (volatile uint16_t i = 0; i < 10000; ++i);
      }
    }

    static void cmd(uint8_t c) {
      DcPin::off();    // D/C=0 → command
      CsPin::off();
      SpiMaster::transfer(&c, 1);
      CsPin::on();
    }

    static void data(const uint8_t* buf, uint8_t len) {
      DcPin::on();     // D/C=1 → data
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

  /// @brief Nokia 5110 PCD8544 84×48 LCD driver; streams characters to display via Transport
  template<typename Transport, uint8_t Contrast = 0x3F>
  struct PCD8544 {
    template<typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;

      static constexpr uint8_t kWidth  = 84;
      static constexpr uint8_t kHeight = 48;
      static constexpr uint8_t kPages  = 6;   // kHeight / 8

      inline static uint8_t _col  = 0;
      inline static uint8_t _page = 0;
      inline static uint8_t _inv  = 0x00;     // 0x00=normal, 0xFF=inverted XOR mask

      static void begin() {
        Transport::begin();
        Transport::cmd(0x21);               // extended instruction set (H=1)
        Transport::cmd(0x80 | Contrast);   // set Vop (contrast 0x80..0xFF)
        Transport::cmd(0x04);              // temperature coefficient TC=0
        Transport::cmd(0x14);             // bias system 1:48 (BS=4)
        Transport::cmd(0x20);              // back to basic instruction set (H=0)
        Transport::cmd(0x0C);              // display on, normal mode
        clear();
        Base::begin();
      }

      static void setInverted(bool v) { _inv = v ? 0xFF : 0x00; }

      static void clear() {
        _set_pos(0, 0);
        Transport::fill(0x00, uint16_t(kWidth) * kPages);
        _col = 0; _page = 0;
      }

      static void setCursor(uint8_t col, uint8_t page) {
        _col = col; _page = page;
        _set_pos(col, page);
      }

      static constexpr uint8_t charWidth()   { return 6; }  // 5px + 1px gap
      static constexpr uint8_t lineSpacing() { return 1; }  // 1 page = 8px

      static void print(char c) {
        if (c == '\n') {
          _page = uint8_t((_page + 1) % kPages);
          _col  = 0;
          _set_pos(_col, _page);
          return;
        }
        if (_col + 6 > kWidth) {
          _page = uint8_t((_page + 1) % kPages);
          _col  = 0;
          _set_pos(_col, _page);
        }
        const uint8_t* g = font5x8_glyph(c);
        uint8_t buf[6];
        for (uint8_t i = 0; i < 5; i++) buf[i] = font5x8_byte(g + i) ^ _inv;
        buf[5] = _inv;  // 1px gap
        Transport::data(buf, 6);
        _col += 6;
      }

      static void print(const char* s) { while (*s) print(*s++); }

      // col/w in pixels; page/h in 8-px pages — matches oledOut.h contract
      static void fillRect(uint8_t col, uint8_t page, uint8_t w, uint8_t h, uint8_t byte = 0x00) {
        const uint8_t b = byte ^ _inv;
        for (uint8_t p = page; p < page + h; p++) {
          _set_pos(col, p);
          Transport::fill(b, w);
        }
        _col = uint8_t(col + w); _page = uint8_t(page + h - 1);
      }

      static void draw_col(uint8_t col, uint8_t page, uint8_t bits) {
        _set_pos(col, page);
        Transport::data(&bits, 1);
        _col = uint8_t(col + 1); _page = page;
      }

      // 84px wide — rounded corners not worth implementing; no-op satisfies OledOut contract
      static void drawRoundRect(uint8_t, uint8_t, uint8_t, uint8_t) {}
      static void setBigFont(bool) {}  // fixed font size on Nokia 5110

    private:
      // PCD8544 basic mode: X address (0x80|col) then Y address (0x40|page)
      // X auto-increments on each data byte; Y does NOT auto-increment.
      static void _set_pos(uint8_t col, uint8_t page) {
        Transport::cmd(uint8_t(0x80 | col));
        Transport::cmd(uint8_t(0x40 | page));
      }
    };
  };

  // Ready-to-use SPI alias
  template<typename SpiMaster, typename CsPin, typename DcPin,
           typename RstPin = void, uint8_t Contrast = 0x3F>
  using SpiNokia5110 = hapi::APIOf<LcdDef,
    PCD8544<SpiPcd8544Transport<SpiMaster, CsPin, DcPin, RstPin>, Contrast>>;

} // oneIO::display
