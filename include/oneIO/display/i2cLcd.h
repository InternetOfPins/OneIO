#pragma once
#include <oneIO/display/hd44780.h>
#include <oneBus/i2cGpio.h>

namespace oneIO::display {

  // PCF8574 I2C LCD backpack — standard pin mapping:
  //   P0=RS  P1=RW  P2=EN  P3=BL  P4=D4  P5=D5  P6=D6  P7=D7
  //
  // InitShadow=0x08: backlight ON (P3=1), RW=0 (write mode), rest low.
  // TwiMaster: hw::avr::AvrTwiMaster<> or equivalent.
  // Addr: 0x27 (PCF8574) or 0x3F (PCF8574A).
  template<typename TwiMaster, uint8_t Addr = 0x27>
  struct I2cLcdPins {
    using Port = oneBus::I2cGpio<TwiMaster, Addr, 0x08>;
    using RS   = typename Port::template Pin<0>;
    using EN   = typename Port::template Pin<2>;
    using D4   = typename Port::template Pin<4>;
    using D5   = typename Port::template Pin<5>;
    using D6   = typename Port::template Pin<6>;
    using D7   = typename Port::template Pin<7>;
    // P1 (RW) = 0 always — write mode, never touched by Hd44780
    // P3 (BL) = 1 always — backlight on, never touched by Hd44780
  };

  template<typename TwiMaster, uint8_t Addr = 0x27, uint8_t Cols = 16, uint8_t Rows = 2>
  struct I2cLcd : hapi::APIOf<LcdDef,
    Hd44780<
      typename I2cLcdPins<TwiMaster, Addr>::RS,
      typename I2cLcdPins<TwiMaster, Addr>::EN,
      typename I2cLcdPins<TwiMaster, Addr>::D4,
      typename I2cLcdPins<TwiMaster, Addr>::D5,
      typename I2cLcdPins<TwiMaster, Addr>::D6,
      typename I2cLcdPins<TwiMaster, Addr>::D7
    >> {
    static constexpr uint8_t cols = Cols;
    static constexpr uint8_t rows = Rows;
  };

} // oneIO::display
