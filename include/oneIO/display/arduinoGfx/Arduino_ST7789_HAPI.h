/*
 * HAPI-composed Arduino_GFX adapter, hosted in OneIO -- see README.md in this
 * directory for scope and rationale (moved here from the Arduino_GFX fork's
 * own src/hapi/, which no longer exists, to avoid a real include-path collision
 * with IOP's own HAPI library -- both used to publish a top-level hapi/ folder).
 *
 * Compile-time-composed stand-in for src/display/Arduino_ST7789.{h,cpp}, templated
 * on a concrete Bus type (e.g. hapi_gfx::Arduino_ESP32PAR8_HAPI<...>) instead of
 * holding an Arduino_DataBus*. Every `_bus->foo(...)` call in the original becomes
 * `_bus.foo(...)` here: a direct, inlinable call on a named concrete type, not a
 * vtable dispatch.
 *
 * Register #defines (ST7789_CASET, ST7789_RAMWR, ...) and the init-sequence byte
 * arrays (st7789_type1_init_operations etc.) are reused as-is from
 * <display/Arduino_ST7789.h> (Arduino_GFX, staged alongside this library) --
 * they are read-only data, safe to #include.
 *
 * Drawing primitives (drawLine, drawCircle, drawArc, fillTriangle, classic-font
 * text via Print, ...) come from Arduino_GFX_HAPI<Derived> (see that file) --
 * the same CRTP base every HAPI display in this adapter set can sit on, mirroring
 * how the original Arduino_GFX base class implements these once and every
 * display driver (Arduino_ST7789 included) inherits them for free. This class
 * itself only supplies the low-level hooks (writePixelPreclipped, width(),
 * height(), and fast writeFastHLine/VLine/writeFillRectPreclipped overrides)
 * plus the ST7789-specific init/rotation/addr-window logic.
 */
#pragma once

#include <display/Arduino_ST7789.h>
#include "Arduino_HAPI_BatchOp.h"
#include "Arduino_GFX_HAPI.h"

namespace hapi_gfx
{

  template <typename Bus>
  class Arduino_ST7789_HAPI : public Arduino_GFX_HAPI<Arduino_ST7789_HAPI<Bus>>
  {
  public:
    Arduino_ST7789_HAPI(
        int8_t rst = GFX_NOT_DEFINED, uint8_t r = 0, bool ips = false,
        int16_t w = ST7789_TFTWIDTH, int16_t h = ST7789_TFTHEIGHT,
        uint8_t col_offset1 = 0, uint8_t row_offset1 = 0,
        uint8_t col_offset2 = 0, uint8_t row_offset2 = 0,
        const uint8_t *init_operations = st7789_type1_init_operations,
        size_t init_operations_len = sizeof(st7789_type1_init_operations))
        : _rst(rst), _rotation(r), _ips(ips), _width(w), _height(h),
          _colOffset1(col_offset1), _rowOffset1(row_offset1),
          _colOffset2(col_offset2), _rowOffset2(row_offset2),
          _initOps(init_operations), _initOpsLen(init_operations_len)
    {
    }

    bool begin(int32_t speed = GFX_NOT_DEFINED)
    {
      bool ok = _bus.begin(speed);
      tftInit();
      setRotation(_rotation);
      startWrite();
      writeAddrWindow(0, 0, _width, _height);
      endWrite();
      return ok;
    }

    void setRotation(uint8_t r)
    {
      _rotation = r & 3;
      switch (_rotation)
      {
      case 1:
        _width = _nativeH;
        _height = _nativeW;
        _xStart = _rowOffset1;
        _yStart = _colOffset2;
        break;
      case 2:
        _width = _nativeW;
        _height = _nativeH;
        _xStart = _colOffset2;
        _yStart = _rowOffset2;
        break;
      case 3:
        _width = _nativeH;
        _height = _nativeW;
        _xStart = _rowOffset2;
        _yStart = _colOffset1;
        break;
      default: // case 0
        _width = _nativeW;
        _height = _nativeH;
        _xStart = _colOffset1;
        _yStart = _rowOffset1;
        break;
      }
      _currentX = _currentY = _currentW = _currentH = 0xFFFF;

      uint8_t madctl;
      switch (_rotation)
      {
      case 1:
        madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
        break;
      case 2:
        madctl = ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB;
        break;
      case 3:
        madctl = ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
        break;
      default: // case 0
        madctl = ST7789_MADCTL_RGB;
        break;
      }
      _bus.beginWrite();
      _bus.writeC8D8(ST7789_MADCTL, madctl);
      _bus.endWrite();
    }

    void invertDisplay(bool i)
    {
      _bus.beginWrite();
      _bus.writeCommand((_ips ^ i) ? ST7789_INVON : ST7789_INVOFF);
      _bus.endWrite();
    }

    void displayOn()
    {
      _bus.beginWrite();
      _bus.writeCommand(ST7789_SLPOUT);
      _bus.endWrite();
      delay(ST7789_SLPOUT_DELAY);
    }

    void displayOff()
    {
      _bus.beginWrite();
      _bus.writeCommand(ST7789_SLPIN);
      _bus.endWrite();
      delay(ST7789_SLPIN_DELAY);
    }

    // ── pixel-write hot path: same call sequence as Arduino_TFT/Arduino_ST7789,
    //    every _bus.foo() below is a direct call, not a vtable dispatch ──────

    GFX_INLINE void startWrite() { _bus.beginWrite(); }
    GFX_INLINE void endWrite() { _bus.endWrite(); }

    void writeAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h)
    {
      if ((x != _currentX) || (w != _currentW))
      {
        _currentX = x;
        _currentW = w;
        x += _xStart;
        _bus.writeC8D16D16(ST7789_CASET, x, x + w - 1);
      }
      if ((y != _currentY) || (h != _currentH))
      {
        _currentY = y;
        _currentH = h;
        y += _yStart;
        _bus.writeC8D16D16(ST7789_RASET, y, y + h - 1);
      }
      _bus.writeCommand(ST7789_RAMWR);
    }

    GFX_INLINE void writePixelPreclipped(int16_t x, int16_t y, uint16_t color)
    {
      writeAddrWindow(x, y, 1, 1);
      _bus.write16(color);
    }

    GFX_INLINE void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
      writeAddrWindow(x, y, w, h);
      _bus.writeRepeat(color, (uint32_t)w * h);
    }

    // fast overrides of Arduino_GFX_HAPI's generic pixel-loop defaults --
    // ported verbatim from Arduino_TFT::writeFastVLine/HLine (a clipped,
    // 1-wide/1-tall writeFillRectPreclipped instead of a per-pixel loop).
    // The clipping here is load-bearing, not decorative: callers like
    // writeEllipseHelper/fillTriangle's scanlines pass coordinates that can
    // run outside the screen, and an unclipped fast path would send those
    // out-of-range x/y straight to CASET/RASET.
    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
    {
      if (_ordered_in_range(x, 0, _width - 1) && h)
      {
        if (h < 0) { y += h + 1; h = -h; }
        if (y <= _height - 1)
        {
          int16_t y2 = y + h - 1;
          if (y2 >= 0)
          {
            if (y < 0) { y = 0; h = y2 + 1; }
            if (y2 > _height - 1) h = _height - 1 - y + 1;
            writeFillRectPreclipped(x, y, 1, h, color);
          }
        }
      }
    }

    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
    {
      if (_ordered_in_range(y, 0, _height - 1) && w)
      {
        if (w < 0) { x += w + 1; w = -w; }
        if (x <= _width - 1)
        {
          int16_t x2 = x + w - 1;
          if (x2 >= 0)
          {
            if (x < 0) { x = 0; w = x2 + 1; }
            if (x2 > _width - 1) w = _width - 1 - x + 1;
            writeFillRectPreclipped(x, y, w, 1, color);
          }
        }
      }
    }

    int16_t width() const { return _width; }
    int16_t height() const { return _height; }

  protected:
    void tftInit()
    {
      if (_rst != GFX_NOT_DEFINED)
      {
        pinMode(_rst, OUTPUT);
        digitalWrite(_rst, HIGH);
        delay(100);
        digitalWrite(_rst, LOW);
        delay(ST7789_RST_DELAY);
        digitalWrite(_rst, HIGH);
        delay(ST7789_RST_DELAY);
      }

      _bus.beginWrite();
      _bus.writeCommand(ST7789_SWRESET);
      _bus.endWrite();
      delay(ST7789_RST_DELAY);

      batchOperation(_bus, _initOps, _initOpsLen);

      invertDisplay(false);
    }

  private:
    Bus _bus;

    int8_t _rst;
    uint8_t _rotation;
    bool _ips;
    int16_t _width, _height;
    int16_t _nativeW = _width, _nativeH = _height;
    uint8_t _colOffset1, _rowOffset1, _colOffset2, _rowOffset2;
    const uint8_t *_initOps;
    size_t _initOpsLen;

    int16_t _xStart = 0, _yStart = 0;
    uint16_t _currentX = 0xFFFF, _currentY = 0xFFFF, _currentW = 0xFFFF, _currentH = 0xFFFF;
  };

} // namespace hapi_gfx
