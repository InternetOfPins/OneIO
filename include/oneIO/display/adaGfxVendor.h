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

    // Real Adafruit_GFX custom font swap (GFXfont*, f=nullptr restores the
    // built-in 5x7 font) — a raw device primitive, same shape as setColors/
    // setInverted. Not driven by setBigFont(bool) below (which stays a no-op
    // here): OneMenu's own FontSwitch<...> component (fmt/vendorFont.h) calls
    // this directly, independent of the Font<bool>::Table/BigTitle mechanism.
    // m_customFont tracks which of the two cached heights below applies —
    // see lineHeight()'s own comment for why this is cache-based, not a live
    // per-call query.
    inline static bool     m_customFont       = false;
    inline static uint16_t m_baseFontH        = CharH;
    inline static uint16_t m_customFontH      = CharH;
    inline static uint16_t m_customFontAscent = 0;
    static void setFont(const GFXfont* f) { driver->setFont(f); m_customFont = (f != nullptr); }

    // Real per-font line height, MEASURED ONCE (not queried live) — see
    // measureLineHeight()/primeBaseFontHeight()/primeCustomFontHeight() below.
    // Found on real ST7789 hardware: calling Adafruit_GFX's getTextBounds()
    // (the only public font-metric API — gfxFont/textsize_y are protected, no
    // getter exists) INTERLEAVED with real fillRect()/print() calls mid-frame
    // corrupts the driver's windowed SPI addressing state, visible as a band
    // of garbled black/white striping across the display. getTextBounds()
    // itself isn't unsafe — calling it BETWEEN frames (never while a
    // fillRect/print sequence is in flight) is fine, which is exactly what
    // the prime*() functions below are for: call them once from setup(),
    // before any real menu output starts, then this hot path never touches
    // getTextBounds() again.
    static uint16_t lineHeight() { return m_customFont ? m_customFontH : m_baseFontH; }

    // One-time measurement helper — see lineHeight()'s comment on why this
    // must never run interleaved with real draw calls. sample="Mg" (capital +
    // descender) is a reasonable representative glyph for "how tall is a
    // typical line in the currently-set font" — h is independent of the x,y
    // anchor passed in, so the exact position given here doesn't matter.
    static uint16_t measureLineHeight(const char* sample="Mg") {
      int16_t x1, y1; uint16_t w, h;
      driver->getTextBounds(sample, 0, 100, &x1, &y1, &w, &h);
      return h > 0 ? h : CharH;
    }
    // Ascent: how far ABOVE the baseline the glyph's real top edge sits.
    // Needed because Adafruit_GFX treats setCursor()'s y as TOP-LEFT for the
    // built-in font but as BASELINE for any custom GFXfont — every OneMenu
    // Fmt component (GfxColorFmt included) works in top-left coordinates
    // uniformly, with no notion of "baseline" at all. Without this offset, a
    // custom font's real ascent renders ABOVE m_itemPos.y — found on real
    // hardware: with Title starting at y=0, the title glyphs rendered
    // partially/fully above the physical top edge of the panel, invisible.
    // getTextBounds()'s y1 (top of the real pixel bbox) comes back relative
    // to the 100px arbitrary anchor passed below, so "100 - y1" is the
    // ascent in pixels; y1>=100 (no ascender in the sample) clamps to 0.
    static uint16_t measureAscent(const char* sample="Mg") {
      int16_t x1, y1; uint16_t w, h;
      driver->getTextBounds(sample, 0, 100, &x1, &y1, &w, &h);
      return y1 < 100 ? (uint16_t)(100 - y1) : 0;
    }
    // Call once from setup() while the built-in font is active (before any
    // setFont(customFont) call).
    static void primeBaseFontHeight()   { m_baseFontH   = measureLineHeight(); }
    // Call once from setup() with the real custom font already set via
    // setFont(f) — caller is responsible for restoring the font afterward
    // (setFont(nullptr) to go back to the built-in font before real
    // rendering starts).
    static void primeCustomFontHeight() {
      m_customFontH      = measureLineHeight();
      m_customFontAscent = measureAscent();
    }

    static void print(char c) {
      driver->setTextColor(_inv ? m_bg : m_fg);
      driver->write((uint8_t)c);
    }
    static void print(const char* s) { while (*s) print(*s++); }

    // Baseline correction: see m_customFontAscent's comment above.
    static void setCursor(uint16_t x, uint16_t y) {
      driver->setCursor(x, m_customFont ? (uint16_t)(y + m_customFontAscent) : y);
    }
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

    inline static bool     m_customFont       = false;
    inline static uint16_t m_baseFontH        = CharH;
    inline static uint16_t m_customFontH      = CharH;
    inline static uint16_t m_customFontAscent = 0;
    static void setFont(const GFXfont* f) { driver->setFont(f); m_customFont = (f != nullptr); }

    // Same shape as AdaGfxVendor's own lineHeight()/measureLineHeight()/
    // measureAscent()/prime*() — see that type's comments for the full
    // rationale (cached, not live, to avoid corrupting the driver's SPI
    // addressing state mid-frame; ascent corrects for Adafruit_GFX's
    // baseline-vs-top-left switch under a custom GFXfont).
    static uint16_t lineHeight() { return m_customFont ? m_customFontH : m_baseFontH; }
    static uint16_t measureLineHeight(const char* sample="Mg") {
      int16_t x1, y1; uint16_t w, h;
      driver->getTextBounds(sample, 0, 100, &x1, &y1, &w, &h);
      return h > 0 ? h : CharH;
    }
    static uint16_t measureAscent(const char* sample="Mg") {
      int16_t x1, y1; uint16_t w, h;
      driver->getTextBounds(sample, 0, 100, &x1, &y1, &w, &h);
      return y1 < 100 ? (uint16_t)(100 - y1) : 0;
    }
    static void primeBaseFontHeight()   { m_baseFontH   = measureLineHeight(); }
    static void primeCustomFontHeight() {
      m_customFontH      = measureLineHeight();
      m_customFontAscent = measureAscent();
    }

    static void print(char c) {
      driver->setTextColor(_inv ? m_bg : m_fg);
      driver->write((uint8_t)c);
    }
    static void print(const char* s) { while (*s) print(*s++); }

    static void setCursor(uint16_t x, uint16_t y) {
      driver->setCursor(x, m_customFont ? (uint16_t)(y + m_customFontAscent) : y);
    }
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
