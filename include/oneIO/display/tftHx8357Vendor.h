#pragma once
#include <TFT_HX8357.h>

namespace oneIO::display {

  // Thin, direct wrapper over a real, already-constructed TFT_HX8357 vendor
  // object (github.com/Bodmer/TFT_HX8357 — the standalone, non-Due variant;
  // confirmed via the real header that `class TFT_HX8357 : public Print`,
  // NOT Adafruit_GFX-derived despite the header's own "derived from the
  // Adafruit_GFX library" design-lineage comment, so AdaGfxVendor cannot be
  // reused here). Per Rui's own direction: "just a blind wrap, let them do
  // what they are already doing" — the vendor's own compile-time font/pin
  // config header (User_Setup.h-style, same pattern TFT_eSPI uses) still
  // governs pins/fonts; this wrapper adds nothing on top of a pure
  // pass-through to the real object's own methods.
  //
  // AM4's own real driver for this device (menuIO/TFT_HX8357Out.h) calls
  // exactly: write(ch), setTextColor(color), fillRect(x,y,w,h,color)
  // (standard x/y/w/h form — no corner-coordinate conversion needed, unlike
  // UTFT's own driver), fillScreen(color), setCursor(x,y), drawRect(x,y,w,
  // h,color) — mapped 1:1 below.
  template<uint16_t Width, uint16_t Height, uint8_t CharW, uint8_t CharH,
           uint16_t FgColor = 0xFFFF, uint16_t BgColor = 0x0000>
  struct TftHx8357Vendor {
    static constexpr uint16_t kWidth  = Width;
    static constexpr uint16_t kHeight = Height;

    inline static TFT_HX8357* driver = nullptr;
    inline static bool        _inv   = false;

    static void begin(TFT_HX8357& d) { driver = &d; }

    static void print(char c) {
      driver->setTextColor(_inv ? BgColor : FgColor);
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
    // TFT_HX8357's own driver never draws a round rect either (AM4's real
    // driver only ever draws a plain rect for its cursor box) — r is
    // ignored, disclosed simplification, same idiom AdaGfxVendor already uses.
    static void drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t r) {
      driver->drawRect(x, y, w, CharH, _inv ? FgColor : BgColor);
    }

    static constexpr uint8_t charWidth()   { return CharW; }
    static constexpr uint8_t lineSpacing() { return CharH; }
  };

} // oneIO::display
