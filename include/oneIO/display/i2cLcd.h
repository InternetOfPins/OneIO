#pragma once
#include <oneIO/display/hd44780.h>
#include <oneBus/i2cGpio.h>

namespace oneIO::display {

  // HD44780 character LCD over PCF8574 I2C backpack.
  // Pin numbers are PCF8574 virtual pins (P0–P7).
  // Defaults match the most common backpack wiring:
  //   P0=RS  P1=RW(gnd)  P2=EN  P3=BL(vcc)  P4=D4  P5=D5  P6=D6  P7=D7
  // BL=InitShadow bit: 1<<BL keeps backlight on without a dedicated control pin.
  // Addr: 0x27 (PCF8574) or 0x3F (PCF8574A).
  template<typename TwiMaster,
           uint8_t Addr = 0x27,
           uint8_t Cols = 16, uint8_t Rows = 2,
           uint8_t RS   = 0,
           uint8_t EN   = 2,
           uint8_t D4   = 4, uint8_t D5 = 5, uint8_t D6 = 6, uint8_t D7 = 7,
           uint8_t BL   = 3>
  struct I2cLcd : hapi::APIOf<LcdDef,
    Hd44780<
      typename oneBus::I2cGpio<TwiMaster, Addr, (1<<BL)>::template Pin<RS>,
      typename oneBus::I2cGpio<TwiMaster, Addr, (1<<BL)>::template Pin<EN>,
      typename oneBus::I2cGpio<TwiMaster, Addr, (1<<BL)>::template Pin<D4>,
      typename oneBus::I2cGpio<TwiMaster, Addr, (1<<BL)>::template Pin<D5>,
      typename oneBus::I2cGpio<TwiMaster, Addr, (1<<BL)>::template Pin<D6>,
      typename oneBus::I2cGpio<TwiMaster, Addr, (1<<BL)>::template Pin<D7>
    >> {
    static constexpr uint8_t cols = Cols;
    static constexpr uint8_t rows = Rows;
  };

} // oneIO::display
