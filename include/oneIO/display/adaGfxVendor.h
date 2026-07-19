#pragma once
#include <Adafruit_GFX.h>

namespace oneIO::display {

  // Thin, direct wrapper over a real, already-constructed Adafruit_GFX-
  // derived vendor object (any concrete TFT/OLED driver built on the
  // Adafruit_GFX base — ST7735, ILI9341, SSD1306, MCUFRIEND, etc.) — same
  // "call the mature vendor library directly instead of reimplementing it"
  // choice AM4's own real Menu::adaGfxOut driver makes, minus depending on
  // AM4's own class hierarchy at all. Static-trait bound (begin(
  // Adafruit_GFX&)), same idiom as oneIO::display::U8g2Vendor/am4compat::
  // MenuOutBridge's own binding.
  //
  // Width/Height/CharW/CharH fixed at compile time, same reasoning as
  // U8g2Vendor: Adafruit_GFX's own width()/height() are runtime calls,
  // which can't satisfy OneMenu's compile-time Cursor<CharW,LineH> NTTP
  // slot. FgColor/BgColor are real RGB565 values (16-bit) — GfxFmt's own
  // rendering is fundamentally a 2-color (inverted/not) abstraction (see
  // this file's own use of _inv below, same shape as U8g2Vendor's), so a
  // full-color-palette menu theme isn't reachable through this path — a
  // disclosed, real limitation, not something worth solving generically
  // here (matches the already-established "Color<Cor>/Font<Fnt> cascading
  // table" scope, which itself is a 2-state — selected/not — mechanism).
  //
  // flush(): a no-op by default. Most concrete Adafruit_GFX drivers (real
  // SPI/parallel TFTs — ST7735, ILI9341, etc.) push each draw call
  // immediately, no local RAM framebuffer, nothing to flush. This does NOT
  // cover Adafruit_SSD1306 specifically (genuinely double-buffered, needs
  // an explicit display() call) — Adafruit_GFX's own base class has no
  // virtual display()/flush() method at all (each subclass defines its own
  // non-virtually), so a plain Adafruit_GFX& reference can't reach it
  // generically; a real SSD1306-over-Adafruit_GFX port would need its own
  // narrower wrapper binding the concrete Adafruit_SSD1306 type instead of
  // the polymorphic base (not attempted here — not needed for the
  // TFT-family examples this was built for; OneIO's own native Ssd1306
  // driver already covers the monochrome-OLED case directly, see
  // ssd1306.h, without any vendor library at all).
  //
  // Inversion (selected-item highlight): unlike U8g2Vendor, Adafruit_GFX's
  // own calls already take an explicit color argument every time — no
  // persistent "current draw color" device state to manage — so fillRect/
  // print simply pick FgColor or BgColor directly based on _inv, no extra
  // setDrawColor-style call needed first.
  /// @brief direct thin wrapper over a real Adafruit_GFX-derived vendor object — no reimplementation, no AM4 dependency
  template<uint8_t Width, uint8_t Height, uint8_t CharW, uint8_t CharH,
           uint16_t FgColor = 0xFFFF, uint16_t BgColor = 0x0000>
  struct AdaGfxVendor {
    static constexpr uint8_t kWidth  = Width;
    static constexpr uint8_t kHeight = Height;

    inline static Adafruit_GFX* driver = nullptr;
    inline static bool          _inv   = false;

    // Call once, with a real, already-constructed AND already-begin()'d
    // (init()/begin() varies per concrete driver, so left to the caller)
    // Adafruit_GFX-derived instance.
    static void begin(Adafruit_GFX& d) { driver = &d; }

    static void print(char c) {
      driver->setTextColor(_inv ? BgColor : FgColor);
      driver->write((uint8_t)c);
    }
    static void print(const char* s) { while (*s) print(*s++); }

    static void setCursor(uint8_t x, uint8_t y) { driver->setCursor(x, y); }
    static void setBigFont(bool) {}  // no big/small font toggle — disclosed simplification
    static void clear() { driver->fillScreen(BgColor); }
    static void flush() {}  // see file header comment
    static void setInverted(bool v) { _inv = v; }

    // byte (Ssd1306-specific XOR-fill-value) is ignored — the real fill
    // color is always FgColor/BgColor via _inv instead, see header comment.
    static void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t = 0) {
      driver->fillRect(x, y, w, h, _inv ? FgColor : BgColor);
    }
    static void drawRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t r) {
      driver->fillRoundRect(x, y, w, CharH, r, _inv ? FgColor : BgColor);
    }

    static constexpr uint8_t charWidth()   { return CharW; }
    static constexpr uint8_t lineSpacing() { return CharH; }
  };

} // oneIO::display
