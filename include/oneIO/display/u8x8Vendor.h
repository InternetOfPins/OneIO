#pragma once
#include <U8x8lib.h>

namespace oneIO::display {

  // Thin, direct wrapper over a real, already-constructed U8X8 vendor
  // object — same "call the mature vendor library directly instead of
  // reimplementing it" policy as U8g2Vendor/AdaGfxVendor. Static-trait
  // bound (begin(U8X8&)), same idiom.
  //
  // U8X8's own API is genuinely tile/character-addressed (drawString(x,y,s)
  // takes x/y in character-cell units directly, not pixels — unlike U8G2's
  // own pixel-addressed drawGlyph) — so unlike U8g2Vendor (charWidth()=7,
  // a real pixel count), this wrapper reports charWidth()=1/lineSpacing()=1
  // ("1 unit" = 1 real character cell), letting OledOut's own Cursor<>
  // track positions in the SAME tile units U8X8's real drawString() already
  // expects — no pixel math needed at all, genuinely simpler than the
  // pixel-addressed VendorGfxOut path U8g2Vendor/AdaGfxVendor go through.
  //
  // Width/Height: Cols/Rows are real tile units (pass the real display's
  // getCols()/getRows() for whatever font is in use, e.g. a 6x8 font on an
  // 84x48 panel is 14 cols x 6 rows). kHeight is reported as Rows*8, NOT a
  // real pixel count — oneMenu::OledDisplay<Oled>'s own StaticArea always
  // divides kHeight by 8 (the SSD1306/Ucg "page = 8px" convention it was
  // built around, confirmed directly against oledOut.h before assuming
  // otherwise), so this is a unit-conversion to make that division recover
  // the real row count, not a real pixel dimension. Nothing else in this
  // wrapper (setCursor/fillRect/Cursor<> advancement, all driven by
  // charWidth()=1/lineSpacing()=1) ever reads kHeight, so this has no
  // other effect.
  /// @brief direct thin wrapper over a real U8X8 vendor object — tile-addressed, no reimplementation
  template<uint8_t Cols, uint8_t Rows>
  struct U8x8Vendor {
    static constexpr uint8_t kWidth  = Cols;
    static constexpr uint8_t kHeight = Rows * 8;

    inline static U8X8* driver = nullptr;
    inline static bool  _inv   = false;
    inline static uint8_t _col = 0, _row = 0;

    static void begin(U8X8& d) { driver = &d; }

    static void setCursor(uint8_t col, uint8_t row) { _col = col; _row = row; }
    static void setBigFont(bool) {}
    static void setInverted(bool v) { _inv = v; driver->setInverseFont(v); }

    static void clear() { driver->clear(); _col = 0; _row = 0; }

    static void print(char c) {
      char s[2] = { c, '\0' };
      driver->drawString(_col, _row, s);
      _col++;
    }
    static void print(const char* s) { while(*s) print(*s++); }

    // No real per-cell fill primitive in U8X8's own API (it's a text-only
    // tile display) — approximate with spaces, matching this display
    // class's own text-only nature (no pixel rects to draw in the first
    // place).
    static void fillRect(uint8_t col, uint8_t row, uint8_t w, uint8_t h, uint8_t = 0) {
      for(uint8_t r = 0; r < h; r++) {
        setCursor(col, row + r);
        for(uint8_t c = 0; c < w; c++) print(' ');
      }
    }
    static void drawRoundRect(uint8_t, uint8_t, uint8_t, uint8_t) {}

    static constexpr uint8_t charWidth()   { return 1; }
    static constexpr uint8_t lineSpacing() { return 1; }
  };

} // oneIO::display
