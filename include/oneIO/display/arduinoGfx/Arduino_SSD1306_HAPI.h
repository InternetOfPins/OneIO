/*
 * HAPI-composed Arduino_GFX adapter, hosted in OneIO -- see README.md in this
 * directory for scope and rationale (moved here from the Arduino_GFX fork's
 * own src/hapi/, to avoid a real include-path collision with IOP's own
 * HAPI library (both name a top-level hapi/ folder).
 *
 * Compile-time-composed stand-in for src/display/Arduino_SSD1306.{h,cpp},
 * templated on `Bus` (e.g. hapi_gfx::Arduino_Wire_HAPI<...>, an I2C bus)
 * instead of holding an Arduino_DataBus*. Same protocol, same register
 * #defines and page/column-addressing sequence reused from
 * <display/Arduino_SSD1306.h> (Arduino_GFX, staged alongside this library) --
 * only the dispatch mechanism changes.
 *
 * W/H (display resolution) are template parameters here rather than
 * constructor arguments: the original computes comPins/_contrast/_colStart/
 * _colEnd from WIDTH/HEIGHT with an if/else chain evaluated at runtime on
 * every begin(); with W/H known at compile time this whole chain collapses
 * to `if constexpr`, so exactly one branch's worth of code exists in the
 * built binary per (W,H) instantiation, not all six checked at runtime.
 *
 * Scope: like Arduino_ST7789_HAPI, this mirrors the original's own surface
 * (begin/drawBitmap/displayOn/displayOff/setBrightness) -- it does not add
 * pixel/text drawing (the original doesn't have any either: Arduino_SSD1306
 * only accepts a pre-rendered 1bpp bitmap via drawBitmap, expecting an
 * Arduino_Canvas_Mono framebuffer above it to do the actual rasterizing).
 * The OneMenu integration example under IOP drives the page/column protocol
 * directly instead, since that's a very different (direct-to-hardware,
 * no local framebuffer) shape than drawBitmap's "blit a whole prepared
 * buffer" contract -- see that example's own adapter code and comments.
 */
#pragma once

#include <display/Arduino_SSD1306.h>
#include "Arduino_HAPI_BatchOp.h"

namespace hapi_gfx
{

  template <typename Bus, int16_t W = 128, int16_t H = 64>
  class Arduino_SSD1306_HAPI
  {
    static constexpr int16_t PAGES = (H + 7) / 8;

    // same six-way resolution table as Arduino_SSD1306::begin(), resolved at
    // compile time instead of a runtime if/else chain re-evaluated every call.
    static constexpr uint8_t comPins()
    {
      if constexpr (W == 128 && H == 64) return 0x12;
      else if constexpr (W == 128 && H == 32) return 0x02;
      else if constexpr (W == 96 && H == 16) return 0x02;
      else if constexpr (W == 72 && H == 40) return 0x12;
      else if constexpr (W == 64 && H == 48) return 0x12;
      else if constexpr (W == 64 && H == 32) return 0x12;
      else return 0x12;
    }
    static constexpr uint8_t contrast()
    {
      if constexpr (W == 128 && H == 64) return 0x9F;
      else if constexpr (W == 128 && H == 32) return 0x8F;
      else if constexpr (W == 96 && H == 16) return 0x10;
      else if constexpr (W == 72 && H == 40) return 0x82;
      else if constexpr (W == 64 && H == 48) return 0x82;
      else if constexpr (W == 64 && H == 32) return 0x82;
      else return 0xCF;
    }
    static constexpr uint8_t colStart()
    {
      if constexpr (W == 72) return 28;
      else if constexpr (W == 64) return 32;
      else return 0;
    }
    static constexpr uint8_t colEnd() { return colStart() + W - 1; }

  public:
    Arduino_SSD1306_HAPI(int8_t rst = GFX_NOT_DEFINED) : _rst(rst) {}

    bool begin(int32_t speed = GFX_NOT_DEFINED)
    {
      if (speed != GFX_SKIP_DATABUS_BEGIN)
      {
        if (!_bus.begin(speed))
          return false;
      }

      if (_rst != GFX_NOT_DEFINED)
      {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH);
        delay(20);
        digitalWrite(_rst, LOW);
        delay(20);
        digitalWrite(_rst, HIGH);
        delay(20);
      }

      const uint8_t init_sequence[] = {
          BEGIN_WRITE,
          WRITE_COMMAND_BYTES, 25,
          SSD1306_DISPLAYOFF,
          SSD1306_SETCONTRAST, contrast(),
          SSD1306_NORMALDISPLAY,
          SSD1306_DEACTIVATE_SCROLL,
          SSD1306_MEMORYMODE, 0x00,
          SSD1306_SEGREMAPINV,
          SSD1306_SETMULTIPLEX, uint8_t(H - 1),
          SSD1306_COMSCANDEC,
          SSD1306_SETDISPLAYOFFSET, 0x00,
          SSD1306_SETCOMPINS, comPins(),
          SSD1306_SETDISPLAYCLOCKDIV, 0x80,
          SSD1306_SETPRECHARGE, 0x22,
          SSD1306_SETVCOMDETECT, 0x40,
          SSD1306_CHARGEPUMP, 0x14,
          uint8_t(SSD1306_SETSTARTLINE | 0x0),
          SSD1306_DISPLAYALLON_RESUME,
          END_WRITE,

          DELAY, 100,

          BEGIN_WRITE,
          WRITE_COMMAND_8, SSD1306_DISPLAYON,
          END_WRITE};

      batchOperation(_bus, init_sequence, sizeof(init_sequence));

      return true;
    }

    void setBrightness(uint8_t brightness)
    {
      _bus.beginWrite();
      _bus.writeC8D8(SSD1306_SETCONTRAST, brightness);
      _bus.endWrite();
    }

    // bitmap must already be packed in SSD1306 page/column order, same as
    // Arduino_SSD1306::drawBitmap -- an Arduino_Canvas_Mono-equivalent
    // framebuffer layer is out of scope here (see file header comment).
    void drawBitmap(uint8_t *bitmap, int16_t w, int16_t h)
    {
      uint16_t count = w * ((h + 7) / 8);

      _bus.beginWrite();
      uint8_t page_sequence[] = {
          SSD1306_PAGEADDR, 0, 0xFF,
          SSD1306_COLUMNADDR, colStart(), colEnd()};
      _bus.writeCommandBytes(page_sequence, sizeof(page_sequence));
      _bus.endWrite();

      _bus.beginWrite();
      _bus.writeBytes(bitmap, count);
      _bus.endWrite();
    }

    void invertDisplay(bool i)
    {
      _bus.beginWrite();
      _bus.writeCommand(i ? SSD1306_INVERSEDISPLAY : SSD1306_NORMALDISPLAY);
      _bus.endWrite();
    }

    void displayOn() { _bus.beginWrite(); _bus.writeCommand(SSD1306_DISPLAYON); _bus.endWrite(); }
    void displayOff() { _bus.beginWrite(); _bus.writeCommand(SSD1306_DISPLAYOFF); _bus.endWrite(); }

    static constexpr int16_t width() { return W; }
    static constexpr int16_t height() { return H; }
    static constexpr int16_t pages() { return PAGES; }

    Bus &bus() { return _bus; }

  private:
    Bus _bus;
    int8_t _rst;
  };

} // namespace hapi_gfx
