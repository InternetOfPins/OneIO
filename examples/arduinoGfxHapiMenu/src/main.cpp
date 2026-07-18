/**
 * @brief OneMenu driven through the Arduino_GFX fork's HAPI-composed I2C bus.
 *
 * Arduino_GFX HAPI supplies the real hardware: hapi_gfx::Arduino_Wire_HAPI
 * (compile-time I2C address/control-byte bus, see
 * Arduino_GFX/src/hapi/Arduino_Wire_HAPI.h) does the actual register writes.
 * OneIO/OneMenu supply the rest, entirely unchanged: Ssd1306<> (font
 * rendering, print/fillRect, already OledOut-compatible) and OledDisplay<>/
 * GfxFmt<> (the OneMenu output chain, proven on real AVR hardware by the
 * u8g2Oled example). oneIO/display/arduinoGfxHapiSsd1306.h is the ~40-line
 * bridge connecting the two: an Ssd1306 Transport backed by the HAPI bus
 * instead of OneBus's TwiMaster.
 *
 * Hardware: any ESP32 devkit + SSD1306 128x64 I2C OLED on the default I2C
 * pins (SDA=21, SCL=22). Navigation over USB serial (arrow keys, Enter/Esc).
 */
#include <Arduino.h>

#include <hapi/hapi.h>
#include <hapi/Arduino_Wire_HAPI.h>
#include <oneData/oneData.h>
#include <oneItem/oneItem.h>
#include <oneOutput/oneOutput.h>
#include <oneMenu/oneMenu.h>
#include <oneMenu/menu/IO/arduino/serialIn.h>
#include <oneMenu/menu/IO/IOP/oledOut.h>
#include "gfxHapiTransport.h"

using namespace hapi;
using namespace oneData;
using namespace oneMenu;

// SSD1306 I2C: address 0x3C, command control-byte 0x00, data control-byte 0x40
// (standard SSD1306 protocol bytes -- same values Arduino_GFX's own
// Arduino_Wire-based SSD1306 examples and OneIO's I2cSsd1306Transport use).
using Bus  = hapi_gfx::Arduino_Wire_HAPI<0x3C, 0x00, 0x40>;
using Oled = oneIO::display::ArduinoGfxHapiSsd1306<Bus, 128, 64>;

// OledDisplay<> / GfxFmt<> / OledOut<> are all existing, unmodified OneMenu
// machinery -- Oled satisfies the same contract u8g2Oled's driver does.
using Display = OledDisplay<Oled>;
Display display;

namespace text {
  static constexpr CText title{"GFX HAPI demo"};
  static constexpr CText count{"Count"};
}

using Count = NumFieldDef<
  AsLabel<StaticText<&text::count>>,
  NumField<
    StaticNumRange<StaticRange<0, 100, false>>,
    AsField<Watch<Default<Int, 10>>>
  >
>;

auto mainMenu = menuDef<WrapNav>(
  ItemDef<StaticText<&text::title>>{},
  staticBody(
    Count{}
  )
);

NavDef<TreeNav, Root<decltype(mainMenu), mainMenu>> nav;
InDef<SerialIn> in;

void setup() {
  Serial.begin(115200);
  Oled::begin();
  nav.printTo(display);
}

void loop() {
  in.doInput(nav);
  nav.doOutput(display);
  delay(20);
}
