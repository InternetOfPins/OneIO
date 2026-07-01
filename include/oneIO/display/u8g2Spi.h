#pragma once
#include <hapi/hapi.h>  // pulls std::is_same polyfill on AVR (no <type_traits> there)
#include <stdint.h>
#include <oneChip/clock.h>

extern "C" {
  #include <clib/u8g2.h>
}

namespace oneIO::display {

  namespace u8g2_detail {
#ifndef U8G2_DELAY_MS
#  define U8G2_DELAY_MS(ms) hw::delay_ms(ms)
#endif

    // GPIO + delay callback — CS/DC/RESET pins and millisecond waits.
    // Matches the message set u8x8_cad_001 (the standard 4-wire-SPI CAD
    // procedure) actually issues: see u8x8_byte_arduino_hw_spi in U8x8lib.cpp
    // for the reference implementation this mirrors.
    template<typename CsPin, typename DcPin, typename RstPin>
    uint8_t gpioAndDelay(u8x8_t*, uint8_t msg, uint8_t arg_int, void*) {
      switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
          CsPin::begin(); CsPin::on();  // CS idle-high (deselected)
          DcPin::begin();
          if constexpr (!std::is_same<RstPin, void>::value) { RstPin::begin(); RstPin::on(); }
          break;
        case U8X8_MSG_GPIO_CS:    if (arg_int) CsPin::on(); else CsPin::off(); break;
        case U8X8_MSG_GPIO_DC:    if (arg_int) DcPin::on(); else DcPin::off(); break;
        case U8X8_MSG_GPIO_RESET:
          if constexpr (!std::is_same<RstPin, void>::value) { if (arg_int) RstPin::on(); else RstPin::off(); }
          break;
        case U8X8_MSG_DELAY_MILLI: U8G2_DELAY_MS(arg_int); break;
        // sub-millisecond delays (10MICRO/100NANO/NANO): no-op — our SPI clock
        // is slow enough relative to these panels that the margin is generous.
        default: break;
      }
      return 1;
    }

    // Byte transport — hardware SPI, 4-wire (separate DC pin). CS/DC toggling
    // is delegated back through gpio_and_delay_cb via the u8x8_gpio_* macros,
    // so this is the only place pin logic lives.
    template<typename SpiMaster>
    uint8_t byteHwSpi(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
      switch (msg) {
        case U8X8_MSG_BYTE_SEND: {
          auto* data = static_cast<uint8_t*>(arg_ptr);
          while (arg_int--) SpiMaster::transfer(*data++);
          break;
        }
        case U8X8_MSG_BYTE_INIT:          SpiMaster::begin();        break;
        case U8X8_MSG_BYTE_SET_DC:        u8x8_gpio_SetDC(u8x8, arg_int); break;
        case U8X8_MSG_BYTE_START_TRANSFER: u8x8_gpio_SetCS(u8x8, 0); break;  // select
        case U8X8_MSG_BYTE_END_TRANSFER:   u8x8_gpio_SetCS(u8x8, 1); break;  // deselect
        default: return 0;
      }
      return 1;
    }
  } // u8g2_detail

  // Wraps a u8g2 SPI display Setup function as an OledOut-compatible IOP device
  // (print/setCursor/fillRect/clear/setInverted/charWidth/lineSpacing), plus
  // renderPages() for tile-buffered setups.
  //
  // Setup is one of u8g2's u8g2_Setup_xxx functions — its "_1"/"_2"/"_f" suffix
  // picks the internal tile-buffer height (in 8px rows). Smaller tiles mean less
  // RAM but more redraw passes: renderPages() must re-run the *whole* frame's
  // draw calls once per tile (u8g2_FirstPage/NextPage), so the caller wraps its
  // full nav.printTo(display) call in renderPages(), not individual prints:
  //
  //   Display::renderPages([]{ nav.printTo(display); });
  //
  // SpiMaster must provide begin()/transfer(uint8_t). CsPin/DcPin/RstPin:
  // begin()/on()/off(). RstPin=void skips hardware reset control.
  /// @brief u8g2 SPI display adapter — OledOut-compatible + tile-buffer FirstPage/NextPage loop
  template<void (*Setup)(u8g2_t*, const u8g2_cb_t*, u8x8_msg_cb, u8x8_msg_cb),
           typename SpiMaster, typename CsPin, typename DcPin, typename RstPin,
           uint8_t Width, uint8_t Height,
           const uint8_t* Font = u8g2_font_5x8_tf>
  struct U8g2Spi {
    struct GfxDef {
      GfxDef() = delete;
      static void begin() {}
    };

    template<typename O>
    struct Part : O {
      static constexpr uint8_t kWidth  = Width;
      static constexpr uint8_t kHeight = Height;

      inline static u8g2_t u8g2;
      inline static uint8_t _col  = 0;
      inline static uint8_t _page = 0;
      inline static bool    _inv  = false;

      static void begin() {
        Setup(&u8g2, U8G2_R0,
              &u8g2_detail::byteHwSpi<SpiMaster>,
              &u8g2_detail::gpioAndDelay<CsPin, DcPin, RstPin>);
        u8g2_InitDisplay(&u8g2);
        u8g2_SetPowerSave(&u8g2, 0);
        u8g2_SetFont(&u8g2, Font);
        u8g2_SetFontPosTop(&u8g2);
        clear();
        O::begin();
      }

      // Re-issues drawFn once per internal tile; required for tile-buffered
      // (non "_f") setups. For "_f" (full framebuffer) this runs drawFn once.
      template<typename Fn>
      static void renderPages(Fn&& drawFn) {
        u8g2_FirstPage(&u8g2);
        do { drawFn(); } while (u8g2_NextPage(&u8g2));
      }

      static void setInverted(bool v) {
        _inv = v;
        u8g2_SetDrawColor(&u8g2, v ? 0 : 1);
      }
      static void setBigFont(bool) {}  // not supported by this thin adapter

      static constexpr uint8_t charWidth()  { return 6; }  // 5px glyph + 1px gap
      static constexpr uint8_t lineSpacing() { return 1; }  // 1 page = 8px

      static void clear() {
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetDrawColor(&u8g2, _inv ? 0 : 1);
        _col = 0; _page = 0;
      }

      // col in pixels, row in pages (8px each) — matches the OledOut contract.
      static void setCursor(uint8_t col, uint8_t page) { _col = col; _page = page; }

      static void print(char c) {
        if (c == '\n') { ++_page; _col = 0; return; }
        u8g2_DrawGlyph(&u8g2, _col, uint8_t(_page * 8), uint8_t(c));
        _col += 6;
      }
      static void print(const char* s) { while (*s) print(*s++); }

      static void fillRect(uint8_t col, uint8_t page, uint8_t w, uint8_t h_pages,
                            uint8_t byte = 0x00) {
        if (!w || !h_pages) return;
        bool isBg = (byte == 0x00);
        u8g2_SetDrawColor(&u8g2, (isBg == _inv) ? 1 : 0);
        u8g2_DrawBox(&u8g2, col, uint8_t(page * 8), w, uint8_t(h_pages * 8));
        u8g2_SetDrawColor(&u8g2, _inv ? 0 : 1);  // restore default draw color for print()
      }

      static void drawRoundRect(uint8_t col, uint8_t page, uint8_t w, uint8_t /*r*/) {
        u8g2_DrawFrame(&u8g2, col, uint8_t(page * 8), w, 8);
      }
    };
  };

  // Ready-to-use assembled u8g2 SPI display. RstPin=void skips reset control.
  template<void (*Setup)(u8g2_t*, const u8g2_cb_t*, u8x8_msg_cb, u8x8_msg_cb),
           typename SpiMaster, typename CsPin, typename DcPin, typename RstPin = void,
           uint8_t Width = 128, uint8_t Height = 64,
           const uint8_t* Font = u8g2_font_5x8_tf>
  using U8g2SpiDisplay = hapi::APIOf<
    typename U8g2Spi<Setup, SpiMaster, CsPin, DcPin, RstPin, Width, Height, Font>::GfxDef,
    U8g2Spi<Setup, SpiMaster, CsPin, DcPin, RstPin, Width, Height, Font>
  >;

} // oneIO::display
