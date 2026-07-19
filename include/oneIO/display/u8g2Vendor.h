#pragma once
#include <U8g2lib.h>

namespace oneIO::display {

  // Thin, direct wrapper over a real, already-constructed U8G2 vendor
  // object (any concrete U8G2_xxx display class) — same "call the mature
  // vendor library directly instead of reimplementing it" choice AM4's own
  // real Menu::u8g2Out driver makes, minus depending on AM4's own class
  // hierarchy at all (no Menu:: namespace, no real upstream ArduinoMenu
  // dependency needed). Static-trait bound (begin(U8G2&)), same idiom as
  // am4compat::MenuOutBridge's own binding — every OneMenu raw-device
  // trait is called on the type, not an instance, and the real vendor
  // object is a single global, bound once.
  //
  // Width/Height/CharW/CharH are all fixed at compile time (matching the
  // panel and font actually loaded via setFont() before begin()) — same
  // convention every real AM4 u8g2-based example already uses (u8g2Out's
  // own resX/resY constructor args, not queried from the U8G2 object at
  // runtime): u8g2's own getMaxCharWidth()/getMaxCharHeight() are runtime
  // calls (font-dependent, can change any time via setFont()), which can't
  // satisfy OneMenu's own compile-time Cursor<CharW,LineH> NTTP slot —
  // fixing the font ahead of time sidesteps that entirely, at the cost of
  // not supporting a sketch that swaps fonts at runtime (not something any
  // real AM4 u8g2 example does anyway).
  //
  // Full-buffer mode, not paged: u8g2's own firstPage()/nextPage() loop
  // (redraw the whole frame N times, once per RAM page) doesn't map onto
  // OneMenu's own incremental put()-based rendering at all — OneMenu draws
  // once per redraw pass, not N times. Full-buffer mode (clearBuffer() +
  // incremental draws + one sendBuffer() at the end) fits directly:
  // sendBuffer() is reached via flush() (VendorGfxOut<Oled>::flush(),
  // oledOut.h), which nav.h already calls exactly once per doOutput()/
  // printTo() pass.
  //
  // Inversion (selected-item highlight): u8g2 has no Ssd1306-style byte-
  // XOR mechanism (Ssd1306::setInverted() toggles a mask XORed into every
  // subsequent byte write, including fillRect's own fill byte) —
  // setDrawColor(0/1) is a plain state-setting call affecting only
  // subsequent draws, not bytes already in the framebuffer. Emulated
  // instead by remembering the _inv flag and setting the draw color
  // separately inside fillRect() (fill color: foreground when inverted) and
  // print() (text color: the OPPOSITE of the fill, so text stays readable
  // on a highlighted row) — same net visual effect GfxFmt's own
  // "setInverted() then fillRect()" call sequence produces on Ssd1306, via
  // a different mechanism (real per-call state, not a byte-level XOR).
  /// @brief direct thin wrapper over a real U8G2 vendor object — no reimplementation, no AM4 dependency
  template<uint8_t Width, uint8_t Height, uint8_t CharW, uint8_t CharH,
           uint8_t FontMarginX = 0, uint8_t FontMarginY = 0>
  struct U8g2Vendor {
    static constexpr uint8_t kWidth  = Width;
    static constexpr uint8_t kHeight = Height;

    inline static U8G2* driver = nullptr;
    inline static bool  _inv   = false;

    // Call once, with a real, already-constructed (but not yet begin()'d)
    // U8G2_xxx instance — this calls the real begin() for you.
    static void begin(U8G2& d) {
      driver = &d;
      driver->begin();
      driver->setFontPosBottom();
    }

    static void print(char c) {
      driver->setDrawColor(_inv ? 0 : 1);
      driver->write((uint8_t)c);
    }
    static void print(const char* s) { while (*s) print(*s++); }

    static void setCursor(uint8_t x, uint8_t y) {
      driver->setCursor(x + FontMarginX, y + FontMarginY);
    }
    static void setBigFont(bool) {}  // no big/small font toggle — disclosed simplification
    static void clear() { driver->clearBuffer(); }
    static void flush() { driver->sendBuffer(); }
    static void setInverted(bool v) { _inv = v; }

    // byte (Ssd1306-specific XOR-fill-value) is ignored here — the actual
    // fill color is always derived from _inv instead, see file header comment.
    static void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t = 0) {
      driver->setDrawColor(_inv ? 1 : 0);
      driver->drawBox(x, y, w, h);
    }
    static void drawRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t r) {
      driver->setDrawColor(_inv ? 1 : 0);
      driver->drawRBox(x, y, w, CharH, r);
    }

    static constexpr uint8_t charWidth()   { return CharW; }
    static constexpr uint8_t lineSpacing() { return CharH; }
  };

} // oneIO::display
