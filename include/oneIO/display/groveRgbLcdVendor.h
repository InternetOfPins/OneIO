#pragma once
#include <rgb_lcd.h>

namespace oneIO::display {

  // Thin, direct wrapper over a real, already-constructed Seeed `rgb_lcd`
  // object (Grove RGB Backlight LCD, its own native I2C protocol — two
  // addresses, one for the HD44780-compatible text controller, one for the
  // RGB backlight PWM controller — genuinely different from the PCF8574-
  // GPIO-expander-driving-HD44780-in-4-bit-mode approach OneIO's own
  // native I2cLcd targets, so not already covered natively). Same
  // vendor-driver-wrapper policy as U8g2Vendor/AdaGfxVendor/U8x8Vendor —
  // calls the real vendor object directly, no reimplementation of its own
  // register protocol. Static-trait bound (begin(rgb_lcd&)).
  //
  // Satisfies oneMenu::LcdOut<Lcd>'s own real contract (lcdOut.h): print(
  // char), print(const char*), setCursor(col,row), clear(), static
  // constexpr cols/rows.
  /// @brief direct thin wrapper over a real Seeed rgb_lcd vendor object
  template<uint8_t Cols, uint8_t Rows>
  struct GroveRgbLcdVendor {
    static constexpr uint8_t cols = Cols;
    static constexpr uint8_t rows = Rows;

    inline static rgb_lcd* driver = nullptr;

    // Call once, with a real, already-constructed rgb_lcd instance — this
    // calls the real begin()/setRGB() for you.
    static void begin(rgb_lcd& d, uint8_t r = 32, uint8_t g = 32, uint8_t b = 32) {
      driver = &d;
      driver->begin(Cols, Rows);
      driver->setRGB(r, g, b);
    }

    static void print(char c)        { driver->write((uint8_t)c); }
    static void print(const char* s) { driver->print(s); }
    static void setCursor(uint8_t col, uint8_t row) { driver->setCursor(col, row); }
    static void clear() { driver->clear(); }
  };

} // oneIO::display
