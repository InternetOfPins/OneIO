/*
 * HAPI-composed Arduino_GFX adapter, hosted in OneIO -- see README.md in this
 * directory for scope and rationale (moved here from the Arduino_GFX fork's
 * own src/hapi/, which no longer exists, to avoid a real include-path collision
 * with IOP's own HAPI library -- both used to publish a top-level hapi/ folder).
 *
 * Compile-time-composed stand-in for src/display/Arduino_ST7735.{h,cpp} --
 * the second display added alongside Arduino_ST7789_HAPI, on the same
 * pattern: templated on `Bus` instead of holding an Arduino_DataBus*.
 * ST7735 shares ST7789's command set (CASET/RASET/RAMWR/MADCTL are the same
 * opcodes) but a different init byte sequence and MADCTL/BGR bit mapping --
 * reused as-is from <display/Arduino_ST7735.h> (Arduino_GFX, staged alongside this library), not retyped.
 */
#pragma once

#include <display/Arduino_ST7735.h>
#include "Arduino_HAPI_BatchOp.h"

namespace hapi_gfx
{

  template <typename Bus>
  class Arduino_ST7735_HAPI
  {
  public:
    Arduino_ST7735_HAPI(
        int8_t rst = GFX_NOT_DEFINED, uint8_t r = 0, bool ips = false,
        int16_t w = ST7735_TFTWIDTH, int16_t h = ST7735_TFTHEIGHT,
        uint8_t col_offset1 = 0, uint8_t row_offset1 = 0,
        uint8_t col_offset2 = 0, uint8_t row_offset2 = 0,
        bool bgr = true)
        : _rst(rst), _rotation(r), _ips(ips), _width(w), _height(h),
          _colOffset1(col_offset1), _rowOffset1(row_offset1),
          _colOffset2(col_offset2), _rowOffset2(row_offset2), _bgr(bgr)
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

      uint8_t bgrBit = _bgr ? ST7735_MADCTL_BGR : ST7735_MADCTL_RGB;
      uint8_t madctl;
      switch (_rotation)
      {
      case 1:
        madctl = ST7735_MADCTL_MY | ST7735_MADCTL_MV | bgrBit;
        break;
      case 2:
        madctl = bgrBit;
        break;
      case 3:
        madctl = ST7735_MADCTL_MX | ST7735_MADCTL_MV | bgrBit;
        break;
      default: // case 0
        madctl = ST7735_MADCTL_MX | ST7735_MADCTL_MY | bgrBit;
        break;
      }
      _bus.beginWrite();
      _bus.writeC8D8(ST7735_MADCTL, madctl);
      _bus.endWrite();
    }

    void invertDisplay(bool i)
    {
      _bus.beginWrite();
      _bus.writeCommand((_ips ^ i) ? ST7735_INVON : ST7735_INVOFF);
      _bus.endWrite();
    }

    void displayOn()
    {
      _bus.beginWrite();
      _bus.writeCommand(ST7735_SLPOUT);
      _bus.endWrite();
      delay(ST7735_SLPOUT_DELAY);
    }

    void displayOff()
    {
      _bus.beginWrite();
      _bus.writeCommand(ST7735_SLPIN);
      _bus.endWrite();
      delay(ST7735_SLPIN_DELAY);
    }

    GFX_INLINE void startWrite() { _bus.beginWrite(); }
    GFX_INLINE void endWrite() { _bus.endWrite(); }

    void writeAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h)
    {
      if ((x != _currentX) || (w != _currentW))
      {
        _currentX = x;
        _currentW = w;
        x += _xStart;
        _bus.writeC8D16D16(ST7735_CASET, x, x + w - 1);
      }
      if ((y != _currentY) || (h != _currentH))
      {
        _currentY = y;
        _currentH = h;
        y += _yStart;
        _bus.writeC8D16D16(ST7735_RASET, y, y + h - 1);
      }
      _bus.writeCommand(ST7735_RAMWR);
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

    void drawPixel(int16_t x, int16_t y, uint16_t color)
    {
      if (x < 0 || x >= _width || y < 0 || y >= _height)
        return;
      startWrite();
      writePixelPreclipped(x, y, color);
      endWrite();
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
      if (w <= 0 || h <= 0)
        return;
      if (x < 0) { w += x; x = 0; }
      if (y < 0) { h += y; y = 0; }
      if (x + w > _width) w = _width - x;
      if (y + h > _height) h = _height - y;
      if (w <= 0 || h <= 0)
        return;
      startWrite();
      writeFillRectPreclipped(x, y, w, h, color);
      endWrite();
    }

    GFX_INLINE void fillScreen(uint16_t color) { fillRect(0, 0, _width, _height, color); }

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
        delay(ST7735_RST_DELAY);
        digitalWrite(_rst, HIGH);
        delay(ST7735_RST_DELAY);
      }
      else
      {
        _bus.beginWrite();
        _bus.writeCommand(ST7735_SWRESET);
        _bus.endWrite();
        delay(ST7735_RST_DELAY);
      }

      batchOperation(_bus, st7735_init_operations, sizeof(st7735_init_operations));

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
    bool _bgr;

    int16_t _xStart = 0, _yStart = 0;
    uint16_t _currentX = 0xFFFF, _currentY = 0xFFFF, _currentW = 0xFFFF, _currentH = 0xFFFF;
  };

} // namespace hapi_gfx
