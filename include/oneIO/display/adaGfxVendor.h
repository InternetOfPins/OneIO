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
  // Width/Height (and the x/y/w/h coordinate params below) are uint16_t,
  // not uint8_t: found 2026-07-25 that any real display taller/wider than
  // 255px (e.g. a 240x320 ILI9341) silently truncated (320 -> 64 mod 256)
  // instead of failing to compile — avr-g++ only warns on this
  // (-Woverflow), it doesn't error, so examples/adafruitGfx_MCUFRIEND's own
  // AdaGfxVendor<320,480,...> had been silently building with
  // Width=64/Height=224 the entire time, never caught. uint16_t covers
  // every real display this wrapper is meant for; CharW/CharH stay uint8_t
  // (character cell sizes are always small).
  //
  // setColors(fg,bg): FgColor/BgColor NTTPs above are now only the *initial*
  // m_fg/m_bg values — a real per-role/state palette is reachable via
  // OneMenu's GfxColorFmt + ColorTable<Color<uint16_t>::Table<...>>, which
  // calls setColors() the same way ANSIFmt calls ANSIOut's setColors(). This
  // lifts the "2-color-only" limitation below for GfxColorFmt users; GfxFmt's
  // own invert-only (_inv) users are unaffected — _inv still swaps between
  // whatever m_fg/m_bg currently hold.
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
  template<uint16_t Width, uint16_t Height, uint8_t CharW, uint8_t CharH,
           uint16_t FgColor = 0xFFFF, uint16_t BgColor = 0x0000>
  struct AdaGfxVendor {
    static constexpr uint16_t kWidth  = Width;
    static constexpr uint16_t kHeight = Height;

    inline static Adafruit_GFX* driver = nullptr;
    inline static bool          _inv   = false;
    inline static uint16_t      m_fg   = FgColor;
    inline static uint16_t      m_bg   = BgColor;

    // Call once, with a real, already-constructed AND already-begin()'d
    // (init()/begin() varies per concrete driver, so left to the caller)
    // Adafruit_GFX-derived instance.
    static void begin(Adafruit_GFX& d) { driver = &d; }

    // Real per-role/state color pair — see file header comment. _inv still
    // swaps between whichever m_fg/m_bg are currently set.
    static void setColors(uint16_t fg, uint16_t bg) { m_fg=fg; m_bg=bg; }

    static void print(char c) {
      driver->setTextColor(_inv ? m_bg : m_fg);
      driver->write((uint8_t)c);
    }
    static void print(const char* s) { while (*s) print(*s++); }

    static void setCursor(uint16_t x, uint16_t y) { driver->setCursor(x, y); }
    static void setBigFont(bool) {}  // no big/small font toggle — disclosed simplification
    static void clear() { driver->fillScreen(m_bg); }
    static void flush() {}  // see file header comment
    static void setInverted(bool v) { _inv = v; }

    // byte (Ssd1306-specific XOR-fill-value) is ignored — the real fill
    // color is always m_fg/m_bg via _inv instead, see header comment.
    static void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t = 0) {
      driver->fillRect(x, y, w, h, _inv ? m_fg : m_bg);
    }
    static void drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t r) {
      driver->fillRoundRect(x, y, w, CharH, r, _inv ? m_fg : m_bg);
    }

    static constexpr uint8_t charWidth()   { return CharW; }
    static constexpr uint8_t lineSpacing() { return CharH; }
  };

  // Buffered variant — for a concrete Adafruit_GFX-derived driver that
  // genuinely double-buffers and needs an explicit display() call after
  // drawing (Adafruit_PCD8544/Nokia 5110, Adafruit_SSD1306, etc.) — the
  // real gap AdaGfxVendor's own header comment above flags: Adafruit_GFX's
  // base class has no virtual display()/flush(), so a plain Adafruit_GFX&
  // can't reach it. Bound to the CONCRETE vendor type (VendorT), not the
  // polymorphic base, so display() resolves as an ordinary non-virtual
  // call — same "flush() reaches the real buffered vendor call" shape as
  // U8g2Vendor's own flush()->sendBuffer().
  template<typename VendorT, uint16_t Width, uint16_t Height, uint8_t CharW, uint8_t CharH,
           uint16_t FgColor = 0xFFFF, uint16_t BgColor = 0x0000>
  struct AdaGfxBufferedVendor {
    static constexpr uint16_t kWidth  = Width;
    static constexpr uint16_t kHeight = Height;

    inline static VendorT*  driver = nullptr;
    inline static bool      _inv   = false;
    inline static uint16_t  m_fg   = FgColor;
    inline static uint16_t  m_bg   = BgColor;

    static void begin(VendorT& d) { driver = &d; }

    static void setColors(uint16_t fg, uint16_t bg) { m_fg=fg; m_bg=bg; }

    static void print(char c) {
      driver->setTextColor(_inv ? m_bg : m_fg);
      driver->write((uint8_t)c);
    }
    static void print(const char* s) { while (*s) print(*s++); }

    static void setCursor(uint16_t x, uint16_t y) { driver->setCursor(x, y); }
    static void setBigFont(bool) {}
    static void clear() { driver->fillScreen(m_bg); }
    static void flush() { driver->display(); }
    static void setInverted(bool v) { _inv = v; }

    static void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t = 0) {
      driver->fillRect(x, y, w, h, _inv ? m_fg : m_bg);
    }
    static void drawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t r) {
      driver->fillRoundRect(x, y, w, CharH, r, _inv ? m_fg : m_bg);
    }

    static constexpr uint8_t charWidth()   { return CharW; }
    static constexpr uint8_t lineSpacing() { return CharH; }
  };

} // oneIO::display
