/*
 * HAPI-composed Arduino_GFX adapter, hosted in OneIO -- see README.md in this
 * directory for scope and rationale (moved here from the Arduino_GFX fork's
 * own src/hapi/, to avoid a real include-path collision with IOP's own
 * HAPI library (both name a top-level hapi/ folder).
 *
 * CRTP stand-in for src/Arduino_GFX.{h,cpp}: the ~30 drawing primitives
 * (lines, rects, circles, ellipses, arcs, triangles, roundrects, classic-font
 * text via Print) that the original library implements exactly ONCE, in this
 * one base class, so that every display driver gets them for free just by
 * providing a handful of low-level hooks (writePixelPreclipped, width(),
 * height(), and optionally faster overrides of writeFastHLine/VLine,
 * writeFillRectPreclipped, writeLine, startWrite/endWrite). A display driver
 * does NOT reimplement drawCircle/drawTriangle/etc in the original -- it
 * inherits them from Arduino_GFX. This file is that same layering, done with
 * CRTP instead of virtual inheritance: `class Arduino_ST7789_HAPI : public
 * Arduino_GFX_HAPI<Arduino_ST7789_HAPI<Bus>>` gets every primitive below for
 * free, and every call from one primitive into another (writeFastHLine calling
 * writePixel, drawRoundRect calling writeEllipseHelper, ...) resolves at
 * compile time through `derived()`, never through a vtable.
 *
 * The algorithms themselves are ported verbatim from Arduino_GFX.cpp (same
 * Bresenham/midpoint-ellipse/scanline-triangle/arc math, same edge cases) --
 * only the dispatch mechanism changes. Classic 5x7 glcdfont text only (no
 * custom GFXfont/u8g2 font support -- out of scope here, same as every other
 * HAPI display in this adapter set not reimplementing the full text engine).
 */
#pragma once

#include <Arduino_GFX.h> // GFX_NOT_DEFINED, _swap_int16_t/_diff/_ordered_in_range, RGB565_* macros
#include <font/glcdfont.h>
#include <Print.h>
#include <float.h>
#include <math.h>

namespace hapi_gfx
{

  template <typename Derived>
  class Arduino_GFX_HAPI : public Print
  {
    Derived &derived() { return static_cast<Derived &>(*this); }

  public:
    // ── hooks with generic defaults; Derived may shadow any of these for speed ──

    void startWrite() {}
    void endWrite() {}

    void writePixel(int16_t x, int16_t y, uint16_t color)
    {
      if (_ordered_in_range(y, 0, derived().height() - 1) && _ordered_in_range(x, 0, derived().width() - 1))
      {
        derived().writePixelPreclipped(x, y, color);
      }
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color)
    {
      derived().startWrite();
      derived().writePixel(x, y, color);
      derived().endWrite();
    }

    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
    {
      for (int16_t i = y; i < y + h; i++)
        derived().writePixel(x, i, color);
    }

    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
    {
      for (int16_t i = x; i < x + w; i++)
        derived().writePixel(i, y, color);
    }

    void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
      for (int16_t i = y; i < y + h; i++)
        derived().writeFastHLine(x, i, w, color);
    }

    void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
      int16_t max_x = derived().width() - 1, max_y = derived().height() - 1;
      if (w && h)
      {
        if (w < 0) { x += w + 1; w = -w; }
        if (x <= max_x)
        {
          if (h < 0) { y += h + 1; h = -h; }
          if (y <= max_y)
          {
            int16_t x2 = x + w - 1;
            if (x2 >= 0)
            {
              int16_t y2 = y + h - 1;
              if (y2 >= 0)
              {
                if (x < 0) { x = 0; w = x2 + 1; }
                if (y < 0) { y = 0; h = y2 + 1; }
                if (x2 > max_x) w = max_x - x + 1;
                if (y2 > max_y) h = max_y - y + 1;
                derived().writeFillRectPreclipped(x, y, w, h, color);
              }
            }
          }
        }
      }
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
      derived().startWrite();
      derived().writeFillRect(x, y, w, h, color);
      derived().endWrite();
    }

    void fillScreen(uint16_t color)
    {
      derived().fillRect(0, 0, derived().width(), derived().height(), color);
    }

    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
    {
      derived().startWrite();
      derived().writeFastVLine(x, y, h, color);
      derived().endWrite();
    }

    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
    {
      derived().startWrite();
      derived().writeFastHLine(x, y, w, color);
      derived().endWrite();
    }

    // ── lines ────────────────────────────────────────────────────────────────

    void writeSlashLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
    {
      bool steep = _diff(y1, y0) > _diff(x1, x0);
      if (steep) { _swap_int16_t(x0, y0); _swap_int16_t(x1, y1); }
      if (x0 > x1) { _swap_int16_t(x0, x1); _swap_int16_t(y0, y1); }

      int16_t dx = x1 - x0;
      int16_t dy = _diff(y1, y0);
      int16_t err = dx >> 1;
      int16_t step = (y0 < y1) ? 1 : -1;

      for (; x0 <= x1; x0++)
      {
        if (steep)
          derived().writePixel(y0, x0, color);
        else
          derived().writePixel(x0, y0, color);
        err -= dy;
        if (err < 0) { err += dx; y0 += step; }
      }
    }

    void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
    {
      if (x0 == x1)
      {
        if (y0 > y1) _swap_int16_t(y0, y1);
        derived().writeFastVLine(x0, y0, y1 - y0 + 1, color);
      }
      else if (y0 == y1)
      {
        if (x0 > x1) _swap_int16_t(x0, x1);
        derived().writeFastHLine(x0, y0, x1 - x0 + 1, color);
      }
      else
      {
        derived().writeSlashLine(x0, y0, x1, y1, color);
      }
    }

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
    {
      derived().startWrite();
      derived().writeLine(x0, y0, x1, y1, color);
      derived().endWrite();
    }

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
    {
      derived().startWrite();
      derived().writeFastHLine(x, y, w, color);
      derived().writeFastHLine(x, y + h - 1, w, color);
      derived().writeFastVLine(x, y, h, color);
      derived().writeFastVLine(x + w - 1, y, h, color);
      derived().endWrite();
    }

    // ── ellipse / circle ────────────────────────────────────────────────────

    void writeEllipseHelper(int32_t x, int32_t y, int32_t rx, int32_t ry, uint8_t cornername, uint16_t color)
    {
      if (rx < 0 || ry < 0 || ((rx == 0) && (ry == 0))) return;
      if (ry == 0) { derived().writeFastHLine(x - rx, y, (ry << 2) + 1, color); return; }
      if (rx == 0) { derived().writeFastVLine(x, y - ry, (rx << 2) + 1, color); return; }

      int32_t xt, yt, s, i;
      int32_t rx2 = rx * rx;
      int32_t ry2 = ry * ry;

      i = -1; xt = 0; yt = ry;
      s = (ry2 << 1) + rx2 * (1 - (ry << 1));
      do
      {
        while (s < 0) s += ry2 * ((++xt << 2) + 2);
        if (cornername & 0x1) derived().writeFastHLine(x - xt, y - yt, xt - i, color);
        if (cornername & 0x2) derived().writeFastHLine(x + i + 1, y - yt, xt - i, color);
        if (cornername & 0x4) derived().writeFastHLine(x + i + 1, y + yt, xt - i, color);
        if (cornername & 0x8) derived().writeFastHLine(x - xt, y + yt, xt - i, color);
        i = xt;
        s -= (--yt) * rx2 << 2;
      } while (ry2 * xt <= rx2 * yt);

      i = -1; yt = 0; xt = rx;
      s = (rx2 << 1) + ry2 * (1 - (rx << 1));
      do
      {
        while (s < 0) s += rx2 * ((++yt << 2) + 2);
        if (cornername & 0x1) derived().writeFastVLine(x - xt, y - yt, yt - i, color);
        if (cornername & 0x2) derived().writeFastVLine(x + xt, y - yt, yt - i, color);
        if (cornername & 0x4) derived().writeFastVLine(x + xt, y + i + 1, yt - i, color);
        if (cornername & 0x8) derived().writeFastVLine(x - xt, y + i + 1, yt - i, color);
        i = yt;
        s -= (--xt) * ry2 << 2;
      } while (rx2 * yt <= ry2 * xt);
    }

    void writeFillEllipseHelper(int32_t x, int32_t y, int32_t rx, int32_t ry, uint8_t corners, int16_t delta, uint16_t color)
    {
      if (rx < 0 || ry < 0 || ((rx == 0) && (ry == 0))) return;
      if (ry == 0) { derived().writeFastHLine(x - rx, y, (ry << 2) + 1, color); return; }
      if (rx == 0) { derived().writeFastVLine(x, y - ry, (rx << 2) + 1, color); return; }

      int32_t xt, yt, i;
      int32_t rx2 = (int32_t)rx * rx;
      int32_t ry2 = (int32_t)ry * ry;
      int32_t s;

      derived().writeFastHLine(x - rx, y, (rx << 1) + 1, color);
      i = 0; yt = 0; xt = rx;
      s = (rx2 << 1) + ry2 * (1 - (rx << 1));
      do
      {
        while (s < 0) s += rx2 * ((++yt << 2) + 2);
        if (corners & 1) derived().writeFillRect(x - xt, y - yt, (xt << 1) + 1 + delta, yt - i, color);
        if (corners & 2) derived().writeFillRect(x - xt, y + i + 1, (xt << 1) + 1 + delta, yt - i, color);
        i = yt;
        s -= (--xt) * ry2 << 2;
      } while (rx2 * yt <= ry2 * xt);

      xt = 0; yt = ry;
      s = (ry2 << 1) + rx2 * (1 - (ry << 1));
      do
      {
        while (s < 0) s += ry2 * ((++xt << 2) + 2);
        if (corners & 1) derived().writeFastHLine(x - xt, y - yt, (xt << 1) + 1 + delta, color);
        if (corners & 2) derived().writeFastHLine(x - xt, y + yt, (xt << 1) + 1 + delta, color);
        s -= (--yt) * rx2 << 2;
      } while (ry2 * xt <= rx2 * yt);
    }

    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color)
    {
      derived().startWrite();
      derived().writeEllipseHelper(x, y, r, r, 0xf, color);
      derived().endWrite();
    }

    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color)
    {
      derived().startWrite();
      derived().writeFillEllipseHelper(x, y, r, r, 3, 0, color);
      derived().endWrite();
    }

    void drawEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry, uint16_t color)
    {
      derived().startWrite();
      derived().writeEllipseHelper(x, y, rx, ry, 0xf, color);
      derived().endWrite();
    }

    void fillEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry, uint16_t color)
    {
      derived().startWrite();
      derived().writeFillEllipseHelper(x, y, rx, ry, 3, 0, color);
      derived().endWrite();
    }

    // ── arc (LovyanGFX-derived, same as upstream) ──────────────────────────

    void writeFillArcHelper(int16_t cx, int16_t cy, int16_t oradius, int16_t iradius, float start, float end, uint16_t color)
    {
      if ((start == 90.0f) || (start == 180.0f) || (start == 270.0f) || (start == 360.0f)) start -= 0.1f;
      if ((end == 90.0f) || (end == 180.0f) || (end == 270.0f) || (end == 360.0f)) end -= 0.1f;

      float s_cos = cosf(start * DEG_TO_RAD);
      float e_cos = cosf(end * DEG_TO_RAD);
      float sslope = s_cos / (sinf(start * DEG_TO_RAD));
      float eslope = e_cos / (sinf(end * DEG_TO_RAD));
      float swidth = 0.5f / s_cos;
      float ewidth = -0.5f / e_cos;
      --iradius;
      int32_t ir2 = iradius * iradius + iradius;
      int32_t or2 = oradius * oradius + oradius;

      bool start180 = !(start < 180.0f);
      bool end180 = end < 180.0f;
      bool reversed = start + 180.0f < end || (end < start && start < end + 180.0f);

      int32_t xs = -oradius;
      int32_t y = -oradius;
      int32_t ye = oradius;
      int32_t xe = oradius + 1;
      if (!reversed)
      {
        if ((end >= 270 || end < 90) && (start >= 270 || start < 90)) xs = 0;
        else if (end < 270 && end >= 90 && start < 270 && start >= 90) xe = 1;
        if (end >= 180 && start >= 180) ye = 0;
        else if (end < 180 && start < 180) y = 0;
      }
      do
      {
        int32_t y2 = y * y;
        int32_t x = xs;
        if (x < 0)
        {
          while (x * x + y2 >= or2) ++x;
          if (xe != 1) xe = 1 - x;
        }
        float ysslope = (y + swidth) * sslope;
        float yeslope = (y + ewidth) * eslope;
        int32_t len = 0;
        do
        {
          bool flg1 = start180 != (x <= ysslope);
          bool flg2 = end180 != (x <= yeslope);
          int32_t distance = x * x + y2;
          if (distance >= ir2 && ((flg1 && flg2) || (reversed && (flg1 || flg2))) && x != xe && distance < or2)
          {
            ++len;
          }
          else
          {
            if (len) { derived().writeFastHLine(cx + x - len, cy + y, len, color); len = 0; }
            if (distance >= or2) break;
            if (x < 0 && distance < ir2) x = -x;
          }
        } while (++x <= xe);
      } while (++y <= ye);
    }

    void drawArc(int16_t x, int16_t y, int16_t r1, int16_t r2, float start, float end, uint16_t color)
    {
      if (r1 < r2) _swap_int16_t(r1, r2);
      if (r1 < 1) r1 = 1;
      if (r2 < 1) r2 = 1;
      bool equal = fabsf(start - end) < FLT_EPSILON;
      start = fmodf(start, 360); end = fmodf(end, 360);
      if (start < 0) start += 360.0f;
      if (end < 0) end += 360.0f;

      derived().startWrite();
      derived().writeFillArcHelper(x, y, r1, r2, start, start, color);
      derived().writeFillArcHelper(x, y, r1, r2, end, end, color);
      if (!equal && (fabsf(start - end) <= 0.0001f)) { start = 0.0f; end = 360.0f; }
      derived().writeFillArcHelper(x, y, r1, r1, start, end, color);
      derived().writeFillArcHelper(x, y, r2, r2, start, end, color);
      derived().endWrite();
    }

    void fillArc(int16_t x, int16_t y, int16_t r1, int16_t r2, float start, float end, uint16_t color)
    {
      if (r1 < r2) _swap_int16_t(r1, r2);
      if (r1 < 1) r1 = 1;
      if (r2 < 1) r2 = 1;
      bool equal = fabsf(start - end) < FLT_EPSILON;
      start = fmodf(start, 360); end = fmodf(end, 360);
      if (start < 0) start += 360.0f;
      if (end < 0) end += 360.0f;
      if (!equal && (fabsf(start - end) <= 0.0001f)) { start = 0.0f; end = 360.0f; }

      derived().startWrite();
      derived().writeFillArcHelper(x, y, r1, r2, start, end, color);
      derived().endWrite();
    }

    // ── roundrect / triangle ────────────────────────────────────────────────

    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
    {
      int16_t max_radius = ((w < h) ? w : h) / 2;
      if (r > max_radius) r = max_radius;
      derived().startWrite();
      derived().writeFastHLine(x + r, y, w - 2 * r, color);
      derived().writeFastHLine(x + r, y + h - 1, w - 2 * r, color);
      derived().writeFastVLine(x, y + r, h - 2 * r, color);
      derived().writeFastVLine(x + w - 1, y + r, h - 2 * r, color);
      derived().writeEllipseHelper(x + r, y + r, r, r, 1, color);
      derived().writeEllipseHelper(x + w - r - 1, y + r, r, r, 2, color);
      derived().writeEllipseHelper(x + w - r - 1, y + h - r - 1, r, r, 4, color);
      derived().writeEllipseHelper(x + r, y + h - r - 1, r, r, 8, color);
      derived().endWrite();
    }

    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
    {
      int16_t max_radius = ((w < h) ? w : h) / 2;
      if (r > max_radius) r = max_radius;
      derived().startWrite();
      derived().writeFillRect(x, y + r, w, h - (r << 1), color);
      derived().writeFillEllipseHelper(x + r, y + r, r, r, 1, w - 2 * r - 1, color);
      derived().writeFillEllipseHelper(x + r, y + h - r - 1, r, r, 2, w - 2 * r - 1, color);
      derived().endWrite();
    }

    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
    {
      derived().startWrite();
      derived().writeLine(x0, y0, x1, y1, color);
      derived().writeLine(x1, y1, x2, y2, color);
      derived().writeLine(x2, y2, x0, y0, color);
      derived().endWrite();
    }

    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
    {
      int16_t a, b, y, last;

      if (y0 > y1) { _swap_int16_t(y0, y1); _swap_int16_t(x0, x1); }
      if (y1 > y2) { _swap_int16_t(y2, y1); _swap_int16_t(x2, x1); }
      if (y0 > y1) { _swap_int16_t(y0, y1); _swap_int16_t(x0, x1); }

      derived().startWrite();
      if (y0 == y2)
      {
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        derived().writeFastHLine(a, y0, b - a + 1, color);
        derived().endWrite();
        return;
      }

      int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0, dx12 = x2 - x1, dy12 = y2 - y1;
      int32_t sa = 0, sb = 0;

      last = (y1 == y2) ? y1 : (y1 - 1);

      for (y = y0; y <= last; y++)
      {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        if (a > b) _swap_int16_t(a, b);
        derived().writeFastHLine(a, y, b - a + 1, color);
      }

      sa = (int32_t)dx12 * (y - y1);
      sb = (int32_t)dx02 * (y - y0);
      for (; y <= y2; y++)
      {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        if (a > b) _swap_int16_t(a, b);
        derived().writeFastHLine(a, y, b - a + 1, color);
      }
      derived().endWrite();
    }

    // ── classic (glcdfont) text ─────────────────────────────────────────────

    void setCursor(int16_t x, int16_t y) { cursor_x = x; cursor_y = y; }
    void setTextColor(uint16_t c) { textcolor = textbgcolor = c; }
    void setTextColor(uint16_t c, uint16_t bg) { textcolor = c; textbgcolor = bg; }
    void setTextWrap(bool w) { wrap = w; }
    void setTextSize(uint8_t s) { textsize_x = textsize_y = (s > 0) ? s : 1; }
    void setTextSize(uint8_t sx, uint8_t sy) { textsize_x = (sx > 0) ? sx : 1; textsize_y = (sy > 0) ? sy : 1; }
    int16_t getCursorX() const { return cursor_x; }
    int16_t getCursorY() const { return cursor_y; }
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
    void flush(bool = false) {} // Canvas-only concept in the original; no-op for direct-write displays

    void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg)
    {
      int16_t block_w = 6 * textsize_x, block_h = 8 * textsize_y;
      int16_t max_x = derived().width() - 1, max_y = derived().height() - 1;
      if ((x > max_x) || (y > max_y) || ((x + block_w - 1) < 0) || ((y + block_h - 1) < 0))
        return;

      derived().startWrite();
      if (textsize_x == 1 && textsize_y == 1)
      {
        int16_t curX = x;
        for (int8_t i = 0; i < 5; ++i, ++curX)
        {
          uint8_t line = font[c * 5 + i];
          if (curX >= 0 && curX <= max_x)
          {
            int16_t curY = y;
            for (int8_t j = 0; j < 8; ++j, ++curY, line >>= 1)
            {
              if (curY >= 0 && curY <= max_y)
              {
                if (line & 1)
                  derived().writePixelPreclipped(curX, curY, color);
                else if (bg != color)
                  derived().writePixelPreclipped(curX, curY, bg);
              }
            }
          }
        }
        if (bg != color)
        {
          int16_t vlX = x + 5;
          if (vlX <= max_x)
          {
            int16_t vlH = ((y + 8 - 1) <= max_y) ? 8 : (max_y - y + 1);
            derived().writeFastVLine(vlX, y, vlH, bg);
          }
        }
      }
      else
      {
        int16_t curX = x;
        for (int8_t i = 0; i < 5; ++i, curX += textsize_x)
        {
          if ((curX + textsize_x - 1) <= max_x)
          {
            uint8_t line = font[c * 5 + i];
            int16_t curY = y;
            for (int8_t j = 0; j < 8; j++, line >>= 1, curY += textsize_y)
            {
              if ((curY + textsize_y - 1) <= max_y)
              {
                if (line & 1)
                  derived().writeFillRect(curX, curY, textsize_x, textsize_y, color);
                else if (bg != color)
                  derived().writeFillRect(curX, curY, textsize_x, textsize_y, bg);
              }
            }
          }
        }
        if (bg != color)
        {
          int16_t vlX = x + 5 * textsize_x;
          if ((vlX + textsize_x - 1) <= max_x)
          {
            int16_t curH = 8 * textsize_y;
            while ((y + curH - 1) > max_y) curH -= textsize_y;
            derived().writeFillRect(vlX, y, textsize_x, curH, bg);
          }
        }
      }
      derived().endWrite();
    }

    // Print interface: gfx->print()/println() call this once per byte.
    size_t write(uint8_t c) override
    {
      if (c == '\n')
      {
        cursor_x = 0;
        cursor_y += textsize_y * 8;
      }
      else if (c != '\r')
      {
        if (wrap && ((cursor_x + (textsize_x * 6) - 1) > (derived().width() - 1)))
        {
          cursor_x = 0;
          cursor_y += textsize_y * 8;
        }
        derived().drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor);
        cursor_x += textsize_x * 6;
      }
      return 1;
    }

  protected:
    int16_t cursor_x = 0, cursor_y = 0;
    uint16_t textcolor = 0xFFFF, textbgcolor = 0xFFFF;
    uint8_t textsize_x = 1, textsize_y = 1;
    bool wrap = true;
  };

} // namespace hapi_gfx
