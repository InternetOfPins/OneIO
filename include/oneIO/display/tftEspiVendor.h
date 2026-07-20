#pragma once
#include <TFT_eSPI.h>

namespace oneIO::display {

  // Thin, direct wrapper over a real, already-constructed TFT_eSPI vendor
  // object (github.com/Bodmer/TFT_eSPI). Confirmed via the real header
  // that `class TFT_eSPI : public Print`, NOT Adafruit_GFX-derived (same
  // situation as TFT_HX8357/UTFT), so AdaGfxVendor cannot be reused here —
  // same "blind wrap, let them do what they are already doing" policy
  // TftHx8357Vendor already established, the vendor's own compile-time
  // User_Setup.h still governs pins/fonts/driver chip.
  //
  // AM4's own real driver for this device (menuIO/TFT_eSPIOut.h) calls
  // exactly: write(ch), setTextColor(fg,bg) (two-arg, unlike
  // TFT_HX8357's single-arg setTextColor), fillRect(x,y,w,h,color)
  // (standard x/y/w/h form — no corner-coordinate conversion needed,
  // unlike UTFT's own driver), fillScreen(color), setCursor(x,y),
  // drawRect(x,y,w,h,color) — mapped 1:1 below.
  template<uint16_t Width, uint16_t Height, uint8_t CharW, uint8_t CharH,
           uint16_t FgColor = 0xFFFF, uint16_t BgColor = 0x0000>
  struct TftEspiVendor {
    static constexpr uint16_t kWidth  = Width;
    static constexpr uint16_t kHeight = Height;

    inline static TFT_eSPI* driver = nullptr;
    inline static bool      _inv   = false;

    static void begin(TFT_eSPI& d) { driver = &d; }

    static void print(char c) {
      driver->setTextColor(_inv ? BgColor : FgColor, _inv ? FgColor : BgColor);
      driver->write((uint8_t)c);
    }
    static void print(const char* s) { while (*s) print(*s++); }

    static void setCursor(uint16_t x, uint16_t y) { driver->setCursor(x, y); }
    static void setBigFont(bool) {}  // no big/small font toggle — disclosed simplification
    static void clear() { driver->fillScreen(BgColor); driver->setCursor(0, 0); }
    static void flush() {}  // direct SPI TFT, no local framebuffer
    static void setInverted(bool v) { _inv = v; }

    static void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t = 0) {
      driver->fillRect(x, y, w, h, _inv ? FgColor : BgColor);
    }
    // TFT_eSPI's own driver never draws a round rect either (AM4's real
    // driver only ever draws a plain rect for its cursor box) — r is
    // ignored, disclosed simplification, same idiom TftHx8357Vendor/
    // AdaGfxVendor already use.
    static void drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t r) {
      driver->drawRect(x, y, w, CharH, _inv ? FgColor : BgColor);
    }

    static constexpr uint8_t charWidth()   { return CharW; }
    static constexpr uint8_t lineSpacing() { return CharH; }
  };

} // oneIO::display
