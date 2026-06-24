#pragma once
#include <hapi/hapi.h>
#include <stdint.h>
#include <oneIO/display/font5x8.h>

namespace oneIO::display {

  struct OledDef {
    OledDef() = delete;
    static void begin() {}
  };

  /// @brief SSD1306 OLED driver (128×64/32); streams characters directly to GDDRAM via Transport
  template<typename Transport, uint8_t Width = 128, uint8_t Height = 64>
  struct Ssd1306 {
    template<typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;

      inline static uint8_t _col      = 0;
      inline static uint8_t _page     = 0;
      inline static uint8_t _inv      = 0x00; // 0x00=normal, 0xFF=inverted XOR mask
      inline static bool    _big_font = false;

    public:
      static constexpr uint8_t kWidth  = Width;
      static constexpr uint8_t kHeight = Height;

      static void setInverted(bool v)  { _inv = v ? 0xFF : 0x00; }
      static void setBigFont(bool v)   { _big_font = v; }

    private:
      // stretch bits 0-3 of b into adjacent pairs → 8-bit page value (top half of 2× glyph)
      static uint8_t _stretch_low(uint8_t b) {
        return uint8_t(((b&1)?0x03u:0u)|((b&2)?0x0Cu:0u)|((b&4)?0x30u:0u)|((b&8)?0xC0u:0u));
      }
      // stretch bits 4-6 of b into adjacent pairs → bits 0-5 (bits 6-7 = gap)
      static uint8_t _stretch_high(uint8_t b) {
        return uint8_t(((b&16)?0x03u:0u)|((b&32)?0x0Cu:0u)|((b&64)?0x30u:0u));
      }

    public:
      // 2× scaled glyph: 10×14px in two pages, 12px advance (10 glyph + 2 gap)
      static void print_big(char c) {
        if(c == '\n') {
          _page = uint8_t((_page + 2) % (Height / 8));
          _col  = 0;
          _set_pos(_col, _page);
          return;
        }
        if(_col + 12 > Width) {
          _page = uint8_t((_page + 2) % (Height / 8));
          _col  = 0;
          _set_pos(_col, _page);
        }
        const uint8_t* g = font5x8_glyph(c);
        uint8_t buf[12];
        // top page: bits 0-3 stretched to 8 bits, each column doubled in width
        for(uint8_t i = 0; i < 5; i++) {
          uint8_t b = _stretch_low(font5x8_byte(g + i)) ^ _inv;
          buf[i*2] = b; buf[i*2+1] = b;
        }
        buf[10] = _inv; buf[11] = _inv;
        Transport::data(buf, 12);
        // bottom page: bits 4-6 stretched to 6 bits (bits 6-7 = gap)
        _set_pos(_col, uint8_t(_page + 1));
        for(uint8_t i = 0; i < 5; i++) {
          uint8_t b = _stretch_high(font5x8_byte(g + i)) ^ _inv;
          buf[i*2] = b; buf[i*2+1] = b;
        }
        buf[10] = _inv; buf[11] = _inv;
        Transport::data(buf, 12);
        _col += 12;
        _set_pos(_col, _page); // return to top page for next character
      }

    private:
      static void _set_pos(uint8_t col, uint8_t page) {
        Transport::cmd(0x21); Transport::cmd(col); Transport::cmd(Width - 1);
        Transport::cmd(0x22); Transport::cmd(page); Transport::cmd(Height / 8 - 1);
      }

    public:
      static void begin() {
        Transport::begin();
        Transport::cmd(0xAE);              // display off
        Transport::cmd(0xD5); Transport::cmd(0x80);              // clock
        Transport::cmd(0xA8); Transport::cmd(Height - 1);        // multiplex
        Transport::cmd(0xD3); Transport::cmd(0x00);              // offset
        Transport::cmd(0x40);              // start line 0
        Transport::cmd(0x8D); Transport::cmd(0x14);              // charge pump ON
        Transport::cmd(0x20); Transport::cmd(0x00);              // horizontal addr mode
        Transport::cmd(0xA1);              // seg remap (flip horizontal)
        Transport::cmd(0xC8);              // COM scan remap (flip vertical)
        Transport::cmd(0xDA); Transport::cmd(Height == 32 ? 0x02u : 0x12u); // COM pins
        Transport::cmd(0x81); Transport::cmd(0xCF);              // contrast
        Transport::cmd(0xD9); Transport::cmd(0xF1);              // pre-charge
        Transport::cmd(0xDB); Transport::cmd(0x40);              // VCOMH
        Transport::cmd(0xA4);              // display follows RAM
        Transport::cmd(0xA6);              // normal (not inverted)
        Transport::cmd(0xAF);              // display on
        clear();
        Base::begin();
      }

      static void clear() {
        _set_pos(0, 0);
        Transport::fill(0x00, (uint16_t)Width * Height / 8);
        _col = 0; _page = 0;
      }

      // col and row are in device-native units: col = pixels (0..Width-1), row = pages (0..Height/8-1)
      static void setCursor(uint8_t col, uint8_t row) {
        _col  = col;
        _page = row;
        _set_pos(_col, _page);
      }
      static constexpr uint8_t charWidth()    { return 6; }  // font5x8: 5px glyph + 1px gap
      static constexpr uint8_t lineSpacing()  { return 1; }  // line advance in pages (1 page = 8px)

      static void print(char c) {
        if(_big_font) { print_big(c); return; }
        if (c == '\n') {
          _page = (_page + 1) % (Height / 8);
          _col  = 0;
          _set_pos(_col, _page);
          return;
        }
        if (_col + 6 > Width) {  // wrap
          _page = (_page + 1) % (Height / 8);
          _col  = 0;
          _set_pos(_col, _page);
        }
        const uint8_t* g = font5x8_glyph(c);
        uint8_t buf[6];
        for (uint8_t i = 0; i < 5; i++) buf[i] = font5x8_byte(g + i) ^ _inv;
        buf[5] = _inv;  // gap: 0x00 normal, 0xFF inverted
        Transport::data(buf, 6);
        _col += 6;
      }

      static void print(const char* s)  { while (*s) print(*s++); }
      static void println(uint8_t row, const char* s) {
        setCursor(0, row); print(s);
      }

      // Raw pixel-column access for custom graphics
      static void draw_col(uint8_t col, uint8_t page, uint8_t bits) {
        _set_pos(col, page);
        Transport::data(&bits, 1);
        _col = col + 1; _page = page;
      }

      // Fill a rectangle with a byte pattern (0x00=black, 0xFF=white).
      // col_pix/w_pix in pixels; page/h_pages in 8-px pages.
      static void fillRect(uint8_t col_pix, uint8_t page, uint8_t w_pix, uint8_t h_pages, uint8_t byte=0x00) {
        const uint8_t b = byte ^ _inv;
        for(uint8_t p=page; p<page+h_pages; p++) {
          _set_pos(col_pix, p);
          Transport::fill(b, w_pix);
        }
        _col=col_pix+w_pix; _page=page+h_pages-1;
      }

      // Draw rounded-rectangle outline for a single-page (8px) tall row.
      // col_pix/w_pix in pixels; page in 8-px pages; r = corner radius in pixels.
      // Sends all column bytes in one I2C transaction via a stack buffer.
      static void drawRoundRect(uint8_t col_pix, uint8_t page, uint8_t w_pix, uint8_t r=2) {
        uint8_t buf[Width];
        const uint8_t n = w_pix <= Width ? w_pix : uint8_t(Width);
        for(uint8_t c=0; c<n; c++) {
          uint8_t dist = c < n/2 ? c : uint8_t(n-1-c);
          uint8_t thresh = r > 0 ? r : uint8_t(1);
          uint8_t byte;
          if(dist < thresh) {
            byte = 0xFF;
            uint8_t trim = r > dist ? uint8_t(r-dist) : uint8_t(0);
            for(uint8_t t=0; t<trim; t++) {
              byte &= ~uint8_t(1u<<t);     // clear top edge bits
              byte &= ~uint8_t(0x80u>>t);  // clear bottom edge bits
            }
          } else {
            byte = 0x81;  // top (bit0) + bottom (bit7) horizontal lines
          }
          buf[c] = byte;
        }
        Transport::cmd(0x21); Transport::cmd(col_pix); Transport::cmd(uint8_t(col_pix+n-1));
        Transport::cmd(0x22); Transport::cmd(page);    Transport::cmd(page);
        Transport::data(buf, n);
        _col=col_pix+n; _page=page;
      }
    };
  };

} // oneIO::display
