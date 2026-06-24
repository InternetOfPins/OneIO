#pragma once
#include <hapi/hapi.h>
#include <tinyTimeUtils.h>
#include <stdint.h>
#include <oneIO/display/font5x8.h>
#include <oneIO/display/spiTft.h>

namespace oneIO::display {

  namespace st7735_detail {
#ifndef ST7735_DELAY_MS
#  define ST7735_DELAY_MS(ms) TinyTimeUtils::ms_delay(ms)
#endif
  }

  // Usage with OneMenu:
  //
  //   using MySpi = hw::esp32::Esp32SpiMaster<>;   // or AVR equivalent
  //   using Cs    = onePin::OutPin<PB2>;
  //   using Dc    = onePin::OutPin<PB1>;
  //   using Rst   = onePin::OutPin<PB0>;
  //   using Tft   = oneIO::display::SpiSt7735<MySpi, Cs, Dc, Rst>;
  //   Tft::begin();
  //   // Direct use:
  //   Tft::print("Hello");
  //   // As OneMenu output:
  //   using TftDisplay = oneMenu::OledDisplay<Tft>;
  //   TftDisplay display;

  // ST7735 / ST7735R 128×160 colour TFT driver.
  //
  // Vertical coordinates use 8-pixel "pages" to match the SSD1306 OledOut contract:
  //   setCursor(col_px, page)          page → pixel_row = page * 8
  //   fillRect(col, page, w, h_pages)  h_pages * 8 actual pixel rows
  //
  // This lets OledDisplay<> and OledOut<> work with this driver without any glue.
  //
  // Transport must provide: begin(), cmd(u8), data(buf, len), data(u8), fill16(u16, count).
  //
  // Madctl: 0x00 = portrait-RGB, 0xC8 = portrait-BGR (common blue-tab module).
  // Xoffset/Yoffset: some smaller-panel variants have a non-zero window origin.
  /// @brief ST7735 SPI TFT driver; OledOut-compatible interface (pages=8px), RGB565
  template<typename Transport,
           uint8_t Width   = 128,
           uint8_t Height  = 160,
           uint8_t Madctl  = 0xC8,
           uint8_t Xoffset = 0,
           uint8_t Yoffset = 0>
  struct St7735 {
    struct TftDef { TftDef() = delete; };
    using Api = hapi::APIOf<TftDef, St7735<Transport, Width, Height, Madctl, Xoffset, Yoffset>>;

    template<typename O>
    struct Part : O {
      static constexpr uint8_t kWidth  = Width;
      static constexpr uint8_t kHeight = Height;

      inline static uint8_t  _col  = 0;   // current column in pixels
      inline static uint8_t  _page = 0;   // current row in pages (8 px each)
      inline static uint16_t _fg   = 0xFFFF;
      inline static uint16_t _bg   = 0x0000;
      inline static bool     _inv  = false;

      static void setInverted(bool v) {
        _inv = v;
        _fg = v ? 0x0000u : 0xFFFFu;
        _bg = v ? 0xFFFFu : 0x0000u;
      }
      static void setBigFont(bool) {}  // not supported on TFT in this driver

      static constexpr uint8_t charWidth()   { return 6; }
      static constexpr uint8_t lineSpacing()  { return 1; }  // 1 page = 8 px

    private:
      // CASET + RASET to define pixel write window, then enter RAMWR
      static void _window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
        uint8_t cb[4];
        // CASET
        Transport::cmd(0x2A);
        cb[0]=0; cb[1]=uint8_t(x0+Xoffset); cb[2]=0; cb[3]=uint8_t(x1+Xoffset);
        Transport::data(cb, 4);
        // RASET
        Transport::cmd(0x2B);
        cb[0]=0; cb[1]=uint8_t(y0+Yoffset); cb[2]=0; cb[3]=uint8_t(y1+Yoffset);
        Transport::data(cb, 4);
        // RAMWR — caller must send pixel data next
        Transport::cmd(0x2C);
      }

    public:
      static void begin() {
        Transport::begin();

        Transport::cmd(0x01);                   // SWRESET
        ST7735_DELAY_MS(150);
        Transport::cmd(0x11);                   // SLPOUT
        ST7735_DELAY_MS(120);

        Transport::cmd(0xB1);                   // FRMCTR1 — normal mode frame rate
        { uint8_t d[]={0x01,0x2C,0x2D}; Transport::data(d, 3); }
        Transport::cmd(0xB2);                   // FRMCTR2 — idle mode
        { uint8_t d[]={0x01,0x2C,0x2D}; Transport::data(d, 3); }
        Transport::cmd(0xB3);                   // FRMCTR3 — partial mode
        { uint8_t d[]={0x01,0x2C,0x2D,0x01,0x2C,0x2D}; Transport::data(d, 6); }

        Transport::cmd(0xB4); Transport::data(0x07);  // INVCTR — no inversion
        Transport::cmd(0xC0);                          // PWCTR1
        { uint8_t d[]={0xA2,0x02,0x84}; Transport::data(d, 3); }
        Transport::cmd(0xC1); Transport::data(0xC5);  // PWCTR2
        Transport::cmd(0xC2);                          // PWCTR3
        { uint8_t d[]={0x0A,0x00}; Transport::data(d, 2); }
        Transport::cmd(0xC3);                          // PWCTR4
        { uint8_t d[]={0x8A,0x2A}; Transport::data(d, 2); }
        Transport::cmd(0xC4);                          // PWCTR5
        { uint8_t d[]={0x8A,0xEE}; Transport::data(d, 2); }
        Transport::cmd(0xC5); Transport::data(0x0E);  // VMCTR1

        Transport::cmd(0x36); Transport::data(Madctl); // MADCTL
        Transport::cmd(0x3A); Transport::data(0x05);   // COLMOD — RGB565

        Transport::cmd(0x13);                          // NORON
        Transport::cmd(0x29);                          // DISPON
        ST7735_DELAY_MS(10);

        clear();
        O::begin();
      }

      static void clear() {
        _window(0, 0, Width-1, Height-1);
        Transport::fill16(_bg, uint32_t(Width)*Height);
        _col = 0; _page = 0;
      }

      // col in pixels, row in pages (8 px each)
      static void setCursor(uint8_t col, uint8_t page) {
        _col  = col;
        _page = page;
      }

      static void print(char c) {
        if (c == '\n') {
          ++_page;
          if (_page >= Height/8) _page = 0;
          _col = 0;
          return;
        }
        if (_col + 6 > Width) {
          ++_page;
          if (_page >= Height/8) _page = 0;
          _col = 0;
        }
        uint8_t py = uint8_t(_page * 8);
        _window(_col, py, uint8_t(_col + 5), uint8_t(py + 7));

        // Blit 6×8 = 48 pixels in one CS transaction.
        // Font is column-major; TFT expects row-major (left→right, top→bottom).
        // Build 96-byte pixel buffer on stack (6 px/row × 8 rows × 2 bytes/px).
        uint8_t buf[96];
        const uint8_t* g = font5x8_glyph(c);
        uint8_t idx = 0;
        for (uint8_t row = 0; row < 8; ++row) {
          for (uint8_t col = 0; col < 5; ++col) {
            uint16_t color = (font5x8_byte(g + col) & (1u << row)) ? _fg : _bg;
            buf[idx++] = uint8_t(color >> 8);
            buf[idx++] = uint8_t(color);
          }
          // gap column — always background
          buf[idx++] = uint8_t(_bg >> 8);
          buf[idx++] = uint8_t(_bg);
        }
        Transport::data(buf, 96);
        _col += 6;
      }

      static void print(const char* s)  { while (*s) print(*s++); }
      static void println(uint8_t row, const char* s) { setCursor(0, row); print(s); }

      // OledOut fillRect interface: col/w in pixels, page/h in pages (8-px units).
      // byte=0x00 → background colour; byte=0xFF → foreground colour.
      static void fillRect(uint8_t col, uint8_t page, uint8_t w, uint8_t h_pages,
                           uint8_t byte = 0x00) {
        if (!w || !h_pages) return;
        uint8_t py = uint8_t(page * 8);
        uint8_t h  = uint8_t(h_pages * 8);
        if (py + h > Height) h = uint8_t(Height - py);
        _window(col, py, uint8_t(col + w - 1), uint8_t(py + h - 1));
        uint16_t color = (byte == 0x00) ? _bg : _fg;
        Transport::fill16(color, uint32_t(w) * h);
      }

      // Lightweight round-rect outline — approximates the SSD1306 version
      // by drawing a 1-page tall rect border without complex curves.
      static void drawRoundRect(uint8_t col, uint8_t page, uint8_t w, uint8_t /*r*/) {
        fillRect(col, page, w, 1, 0xFF);
      }
    };
  };

  // Ready-to-use assembled TFT + transport
  template<typename SpiMaster, typename CsPin, typename DcPin,
           typename RstPin  = void,
           uint8_t Width    = 128,
           uint8_t Height   = 160,
           uint8_t Madctl   = 0xC8,
           uint8_t Xoffset  = 0,
           uint8_t Yoffset  = 0>
  using SpiSt7735 = hapi::APIOf<
    typename St7735<SpiTftTransport<SpiMaster, CsPin, DcPin, RstPin>,
                    Width, Height, Madctl, Xoffset, Yoffset>::TftDef,
    St7735<SpiTftTransport<SpiMaster, CsPin, DcPin, RstPin>,
           Width, Height, Madctl, Xoffset, Yoffset>
  >;

} // oneIO::display
