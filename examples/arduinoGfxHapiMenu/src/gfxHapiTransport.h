#pragma once
// Bridges the Arduino_GFX fork's HAPI-composed I2C bus
// (hapi_gfx::Arduino_Wire_HAPI<I2cAddr,CommandPrefix,DataPrefix>, see
// Arduino_GFX/src/hapi/Arduino_Wire_HAPI.h) into OneIO's published
// Ssd1306<Transport,W,H> driver (font rendering / print / fillRect / OledOut
// compatibility, already used by I2cOled) as its byte-level Transport.
//
// Kept local to this example rather than added to OneIO's own include/ tree:
// it's just a template usage of two independently-published libraries, not a
// piece either library needs to own, and this example already depends on
// unpublished local edits to Arduino_GFX -- no reason to also grow OneIO's
// public surface for a demo.
#include <oneIO/display/ssd1306.h>
#include <hapi/Arduino_Wire_HAPI.h>

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

  // Ready-to-use OledOut-compatible SSD1306, driven by Arduino_GFX HAPI's I2C bus.
  // Bus = hapi_gfx::Arduino_Wire_HAPI<I2cAddr, CommandPrefix, DataPrefix>
  template<typename Bus, uint8_t Width = 128, uint8_t Height = 64>
  using ArduinoGfxHapiSsd1306 = hapi::APIOf<OledDef, Ssd1306<ArduinoGfxHapiSsd1306Transport<Bus>, Width, Height>>;

} // namespace oneIO::display
