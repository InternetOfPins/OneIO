#pragma once
#include <hapi/hapi.h>  // pulls std::is_same polyfill on AVR (no <type_traits> there)
#include <stdint.h>
#include <oneChip/clock.h>

extern "C" {
  #include <clib/ucg.h>
}

namespace oneIO::display {

  namespace ucg_detail {
    // Single HAL callback — Ucglib merges byte-transport + gpio + delay into
    // one com_cb (unlike u8g2's separate byte_cb/gpio_and_delay_cb). Mirrors
    // the message set Ucglib's own reference implementation handles: see
    // ucg_com_arduino_4wire_HW_SPI in Ucglib.cpp.
    template<typename SpiMaster, typename CsPin, typename DcPin, typename RstPin>
    int16_t com(ucg_t*, int16_t msg, uint16_t arg, uint8_t* data) {
      switch (msg) {
        case UCG_COM_MSG_POWER_UP:
          DcPin::begin();
          if constexpr (!std::is_same<CsPin,  void>::value) { CsPin::begin();  CsPin::on();  }
          if constexpr (!std::is_same<RstPin, void>::value) { RstPin::begin(); RstPin::on(); }
          SpiMaster::begin();
          break;
        case UCG_COM_MSG_POWER_DOWN:
          break;
        case UCG_COM_MSG_DELAY:  // arg is in microseconds
#ifdef __AVR__
          while (arg--) _delay_us(1);
#else
          hw::delay_ms((arg + 999) / 1000);
#endif
          break;
        case UCG_COM_MSG_CHANGE_RESET_LINE:
          if constexpr (!std::is_same<RstPin, void>::value) { if (arg) RstPin::on(); else RstPin::off(); }
          break;
        case UCG_COM_MSG_CHANGE_CS_LINE:
          if constexpr (!std::is_same<CsPin, void>::value) { if (arg) CsPin::on(); else CsPin::off(); }
          break;
        case UCG_COM_MSG_CHANGE_CD_LINE:
          if (arg) DcPin::on(); else DcPin::off();
          break;
        case UCG_COM_MSG_SEND_BYTE:
          SpiMaster::transfer(uint8_t(arg));
          break;
        case UCG_COM_MSG_REPEAT_1_BYTE:
          while (arg--) SpiMaster::transfer(data[0]);
          break;
        case UCG_COM_MSG_REPEAT_2_BYTES:
          while (arg--) { SpiMaster::transfer(data[0]); SpiMaster::transfer(data[1]); }
          break;
        case UCG_COM_MSG_REPEAT_3_BYTES:
          while (arg--) { SpiMaster::transfer(data[0]); SpiMaster::transfer(data[1]); SpiMaster::transfer(data[2]); }
          break;
        case UCG_COM_MSG_SEND_STR:
          while (arg--) SpiMaster::transfer(*data++);
          break;
        case UCG_COM_MSG_SEND_CD_DATA_SEQUENCE:
          while (arg > 0) {
            if (*data != 0) { if (*data == 1) DcPin::off(); else DcPin::on(); }
            ++data;
            SpiMaster::transfer(*data);
            ++data;
            --arg;
          }
          break;
        default: break;
      }
      return 1;
    }
  } // ucg_detail

  // Wraps a Ucglib SPI panel as an OledOut-compatible IOP device (so it drops
  // into OneMenu the same way as st7735.h/ssd1306.h), PLUS native RGB
  // primitives (setColor/drawPixel/drawLine/drawDisc/...) for real multi-colour
  // graphics — the thing our hand-rolled TFT drivers can't do, since their
  // fillRect() only picks between one fixed fg and one fixed bg colour.
  //
  // Text is drawn via Ucglib's own font system (ucg_SetFont/ucg_DrawString) —
  // fonts are BSD/free and the linker (-ffunction-sections/--gc-sections)
  // drops every font glyph table except the ones actually referenced, so
  // picking one small font here costs nothing at link time regardless of how
  // many fonts ucg_pixel_font_data.c/ucg_vector_font_data.c bundle.
  //
  // DeviceCb/ExtCb: one of Ucglib's ucg_dev_xxx / ucg_ext_xxx pairs, e.g.
  //   ucg_dev_st7735_18x128x160 + ucg_ext_st7735_18
  // Font: one of Ucglib's ucg_font_xxx tables (not gated behind OLD_FONTS).
  //
  // SpiMaster must provide begin()/transfer(uint8_t). CsPin/DcPin/RstPin:
  // begin()/on()/off(). CsPin/RstPin = void skips that line entirely.
  /// @brief Ucglib SPI TFT adapter — OledOut-compatible + native RGB draw primitives
  template<ucg_dev_fnptr DeviceCb, ucg_dev_fnptr ExtCb,
           typename SpiMaster, typename CsPin, typename DcPin, typename RstPin,
           uint8_t Width, uint8_t Height,
           const ucg_fntpgm_uint8_t* Font = ucg_font_5x8_tf>
  struct UcgSpi {
    struct GfxDef {
      GfxDef() = delete;
      static void begin() {}
    };

    template<typename O>
    struct Part : O {
      static constexpr uint8_t kWidth  = Width;
      static constexpr uint8_t kHeight = Height;

      inline static ucg_t   ucg;
      inline static uint8_t _col  = 0;
      inline static uint8_t _page = 0;
      inline static bool    _inv  = false;
      inline static uint8_t _fg[3] = {255, 255, 255};
      inline static uint8_t _bg[3] = {0, 0, 0};

      static void begin() {
        ucg_Init(&ucg, DeviceCb, ExtCb, &ucg_detail::com<SpiMaster, CsPin, DcPin, RstPin>);
        ucg_SetFont(&ucg, Font);
        ucg_SetFontPosTop(&ucg);
        clear();
        O::begin();
      }

      // ── Native RGB primitives — bypass the char-grid contract entirely ──
      static void setColor(uint8_t idx, uint8_t r, uint8_t g, uint8_t b) { ucg_SetColor(&ucg, idx, r, g, b); }
      static void drawPixel(int16_t x, int16_t y)                        { ucg_DrawPixel(&ucg, x, y); }
      static void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) { ucg_DrawLine(&ucg, x0, y0, x1, y1); }
      static void drawDisc(int16_t x, int16_t y, int16_t r)              { ucg_DrawDisc(&ucg, x, y, r, UCG_DRAW_ALL); }

      // ── OledOut-compatible contract (page-granular, fg/bg) ──
      static void setInverted(bool v) { _inv = v; }
      static void setBigFont(bool) {}  // not supported by this thin adapter

      static constexpr uint8_t charWidth()   { return 6; }  // 5px glyph + 1px gap
      static constexpr uint8_t lineSpacing() { return 1; }  // 1 page = 8px

      static void clear() {
        const uint8_t* bg = _inv ? _fg : _bg;
        ucg_SetColor(&ucg, 0, bg[0], bg[1], bg[2]);
        ucg_DrawBox(&ucg, 0, 0, Width, Height);
        _col = 0; _page = 0;
      }

      // col in pixels, row in pages (8px each) — matches the OledOut contract.
      static void setCursor(uint8_t col, uint8_t page) { _col = col; _page = page; }

      // Solid 6x8 cell: background box first (Font is a transparent variant,
      // so ucg_DrawString only touches foreground pixels), then the glyph on
      // top — so a redraw fully overwrites whatever character was there before.
      static void print(char c) {
        if (c == '\n') { ++_page; _col = 0; return; }
        uint8_t x0 = _col, y0 = uint8_t(_page * 8);
        const uint8_t* bg = _inv ? _fg : _bg;
        ucg_SetColor(&ucg, 0, bg[0], bg[1], bg[2]);
        ucg_DrawBox(&ucg, x0, y0, 6, 8);

        const uint8_t* fg = _inv ? _bg : _fg;
        ucg_SetColor(&ucg, 0, fg[0], fg[1], fg[2]);
        char s[2] = { c, '\0' };
        ucg_DrawString(&ucg, x0, y0, 0, s);
        _col += 6;
      }
      static void print(const char* s) { while (*s) print(*s++); }

      static void fillRect(uint8_t col, uint8_t page, uint8_t w, uint8_t h_pages,
                            uint8_t byte = 0x00) {
        if (!w || !h_pages) return;
        bool isBg = (byte == 0x00);
        const uint8_t* c = (isBg == _inv) ? _fg : _bg;
        ucg_SetColor(&ucg, 0, c[0], c[1], c[2]);
        ucg_DrawBox(&ucg, col, uint8_t(page * 8), w, uint8_t(h_pages * 8));
      }

      static void drawRoundRect(uint8_t col, uint8_t page, uint8_t w, uint8_t /*r*/) {
        ucg_DrawFrame(&ucg, col, uint8_t(page * 8), w, 8);
      }
    };
  };

  // Ready-to-use assembled Ucglib SPI display. RstPin/CsPin = void skip that line.
  template<ucg_dev_fnptr DeviceCb, ucg_dev_fnptr ExtCb,
           typename SpiMaster, typename CsPin, typename DcPin, typename RstPin = void,
           uint8_t Width = 128, uint8_t Height = 160,
           const ucg_fntpgm_uint8_t* Font = ucg_font_5x8_tf>
  using UcgSpiDisplay = hapi::APIOf<
    typename UcgSpi<DeviceCb, ExtCb, SpiMaster, CsPin, DcPin, RstPin, Width, Height, Font>::GfxDef,
    UcgSpi<DeviceCb, ExtCb, SpiMaster, CsPin, DcPin, RstPin, Width, Height, Font>
  >;

} // oneIO::display
