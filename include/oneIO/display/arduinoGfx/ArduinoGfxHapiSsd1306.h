#pragma once
// Bridges hapi_gfx::Arduino_Wire_HAPI<I2cAddr,CommandPrefix,DataPrefix>
// (Arduino_Wire_HAPI.h, in this same directory) into OneIO's own native
// Ssd1306<Transport,W,H> driver (font rendering / print / fillRect / OledOut
// compatibility, already used by I2cOled -- see ../ssd1306.h) as its
// byte-level Transport.
//
// This is the "OneMenu driven through the Arduino_GFX HAPI adapters"
// integration point: Arduino_Wire_HAPI supplies the actual I2C register
// writes, OneIO/OneMenu supply the rest unchanged (Ssd1306<>, OledDisplay<>/
// GfxFmt<>). See OneIO/examples/arduinoGfxMenu/ for a full working
// example, and README.md in this directory for the wider design writeup.
#include <oneIO/display/ssd1306.h>
#include <oneIO/display/arduinoGfx/Arduino_Wire_HAPI.h>

namespace oneIO::display {

  template<typename Bus>
  struct ArduinoGfxHapiSsd1306Transport {
    static inline Bus bus;

    static void begin() { bus.begin(); }

    static void cmd(uint8_t c) {
      bus.beginWrite();
      bus.writeCommand(c);
      bus.endWrite();
    }

    static void data(const uint8_t* buf, uint8_t len) {
      bus.beginWrite();
      bus.writeBytes(const_cast<uint8_t*>(buf), len);
      bus.endWrite();
    }

    // 32-byte chunks, one self-contained I2C transaction each -- fill() has
    // no real buffer, just a repeated byte, so build one and send it in
    // pieces small enough for any TwoWire implementation's I2C buffer.
    static void fill(uint8_t b, uint16_t count) {
      uint8_t chunk[32];
      for (uint8_t i = 0; i < 32; i++) chunk[i] = b;
      while (count) {
        uint8_t n = count > 32 ? 32 : (uint8_t)count;
        bus.beginWrite();
        bus.writeBytes(chunk, n);
        bus.endWrite();
        count = uint16_t(count - n);
      }
    }
  };

  // Ready-to-use OledOut-compatible SSD1306, driven by the Arduino_GFX HAPI
  // adapters' I2C bus. Bus = hapi_gfx::Arduino_Wire_HAPI<I2cAddr, CommandPrefix, DataPrefix>
  template<typename Bus, uint8_t Width = 128, uint8_t Height = 64>
  using ArduinoGfxHapiSsd1306 = hapi::APIOf<OledDef, Ssd1306<ArduinoGfxHapiSsd1306Transport<Bus>, Width, Height>>;

} // namespace oneIO::display
