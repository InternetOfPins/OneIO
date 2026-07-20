#pragma once
#include <UTFT.h>

namespace oneIO::display {

  // Thin, direct wrapper over a real, already-constructed UTFT object
  // (rinkydinkelectronics.com UTFT, Due-only per its own real hardware
  // requirements — non-standard pin wiring, e.g. `UTFT tft(CTE28,25,26,27,
  // 28)`). Three real deviations from every prior GFX wrapper here, all
  // load-bearing:
  //  - UTFT's own fillRect/drawRect take CORNER form (x0,y0,x1,y1), not
  //    x/y/w/h — converted inside fillRect()/drawRoundRect() below
  //    (confirmed via AM4's own real driver, menuIO/utftOut.h).
  //  - UTFT's text draw takes an explicit position every call (no
  //    persistent device-side cursor at all) — this wrapper tracks its own
  //    _x/_y, same idiom U8x8Vendor already uses for _col/_row. Uses the
  //    public print(char*,x,y) (one-char buffer) rather than AM4's own
  //    driver's printChar(ch,x,y) — confirmed against a real, buildable
  //    fork (johncblacker/UTFT) that printChar is `protected`, an internal
  //    helper only print(char*,x,y) itself is meant to call; print(char*,
  //    x,y) is the real public, portable primitive.
  //  - setColor/setBackColor take separate (r,g,b) bytes, not a packed
  //    RGB565 uint16_t (confirmed against a real, buildable UTFT fork,
  //    johncblacker/UTFT — the original rinkydinkelectronics UTFT AM4's
  //    driver was written against also has a uint16_t overload, but that
  //    overload isn't universal across forks, so this wrapper always
  //    unpacks its own uint16_t FgColor/BgColor NTTPs into r,g,b via
  //    rgb565() below, working against either).
  template<uint16_t Width, uint16_t Height, uint8_t CharW, uint8_t CharH,
           uint16_t FgColor = 0xFFFF, uint16_t BgColor = 0x0000>
  struct UtftVendor {
    static constexpr uint16_t kWidth  = Width;
    static constexpr uint16_t kHeight = Height;

    inline static UTFT* driver = nullptr;
    inline static bool  _inv   = false;
    inline static uint16_t _x = 0, _y = 0;

    static void begin(UTFT& d) { driver = &d; }

    static void rgb565(uint16_t c, byte& r, byte& g, byte& b) {
      r = (c >> 11) & 0x1F; r = (r << 3) | (r >> 2);
      g = (c >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
      b =  c        & 0x1F; b = (b << 3) | (b >> 2);
    }

    // '\n' resets _x and advances _y one line — VendorGfxOut's own row-start
    // convention (Cursor<>/FullPrinter) always issues an explicit
    // setCursor() before a row's first char, so a '\n'-only reset is never
    // relied upon between rows in practice; handled here defensively anyway.
    static void print(char c) {
      if (c == '\n') { _x = 0; _y += CharH; return; }
      byte r, g, b;
      rgb565(_inv ? BgColor : FgColor, r, g, b);
      driver->setColor(r, g, b);
      rgb565(_inv ? FgColor : BgColor, r, g, b);
      driver->setBackColor(r, g, b);
      char buf[2] = { c, '\0' };
      driver->print(buf, _x, _y);
      _x += CharW;
    }
    static void print(const char* s) { while (*s) print(*s++); }

    static void setCursor(uint16_t x, uint16_t y) { _x = x; _y = y; }
    static void setBigFont(bool) {}  // no big/small font toggle — disclosed simplification
    static void clear() {
      byte r, g, b;
      rgb565(BgColor, r, g, b);
      driver->setBackColor(r, g, b);
      driver->clrScr();
      _x = _y = 0;
    }
    static void flush() {}  // direct-to-hardware, no local framebuffer
    static void setInverted(bool v) { _inv = v; }

    // AM4's own driver takes CORNER form (x0,y0,x1,y1) — converted here.
    static void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t = 0) {
      byte r, g, b;
      rgb565(_inv ? FgColor : BgColor, r, g, b);
      driver->setColor(r, g, b);
      driver->fillRect(x, y, x + w - 1, y + h - 1);
    }
    // UTFT has no native round-rect primitive at all — AM4's own driver
    // never draws one either (only a plain drawRect for the cursor box) —
    // r is ignored, disclosed simplification, same as TftHx8357Vendor's own.
    static void drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t r) {
      byte cr, cg, cb;
      rgb565(_inv ? FgColor : BgColor, cr, cg, cb);
      driver->setColor(cr, cg, cb);
      driver->drawRect(x, y, x + w - 1, y + CharH - 1);
    }

    static constexpr uint8_t charWidth()   { return CharW; }
    static constexpr uint8_t lineSpacing() { return CharH; }
  };

} // oneIO::display
