// IOP HD44780 16x2 LCD over a PCF8574 I2C backpack.
//   AVR:   SDA=A4, SCL=A5, address 0x27 (0x3F for PCF8574A).
//   STM32: SDA=PB7, SCL=PB6 (I2C1), same address selection.
//          Clock via -DIOP_CPU_HZ: 72000000 (HSE x9) or 64000000 (HSI/2 x16).

#include <oneIO/display/i2cLcd.h>
using namespace oneIO::display;

#ifndef LCD_ADDR
  #define LCD_ADDR 0x27
#endif

#if defined(__arm__)
  #include <chips/stm32/stm32Device.h>
  using namespace onePin;
  using namespace oneBit;
  using namespace hw::stm32;

  #ifndef IOP_CPU_HZ
    #define IOP_CPU_HZ 72000000
  #endif
  // APB1 (I2C1 clock) = HCLK/2 for both PLL configs. hd44780.h's delay
  // loop is calibrated for ~72 MHz; at 64 MHz it over-delays, which the
  // HD44780 tolerates (only under-delay breaks init).
  #if IOP_CPU_HZ == 64000000
    static constexpr uint32_t APB1_HZ = 32000000;  // HSI/2 x16
  #else
    static constexpr uint32_t APB1_HZ = 36000000;  // HSE x9
  #endif

  using Twi   = hw::stm32::chip::Twi<100000, APB1_HZ>;   // I2C1 — PB6 SCL / PB7 SDA
  using Clock = hw::stm32::chip::SysClk<IOP_CPU_HZ>;
  using Board = hw::stm32::STM32::Board<Boot<Clock>>;
  #ifdef IOP
    IOP_SYSTICK_ISR(Board)
  #endif

#else  // AVR bare-metal
  #include <chips/avr/avrDevice.h>
  using namespace onePin;
  using namespace oneBit;
  using namespace hw::avr;

  using Twi   = chip::TwiMaster<>;            // 100 kHz, 16 MHz CPU — SDA=A4 / SCL=A5
  using Clock = chip::SysTick0<>;
  using Board = AVR::Board<Boot<Clock>>;
  #ifdef IOP
    IOP_TIMER0_ISR(Board)
  #endif
#endif

using Lcd = I2cLcd<Twi, LCD_ADDR>;

Clock::Period<1000> period;

static void printNum(uint32_t n) {
  char buf[11];
  uint8_t i = 10;
  buf[10] = '\0';
  if (!n) { Lcd::print('0'); return; }
  while (n && i) { buf[--i] = '0' + (n % 10); n /= 10; }
  Lcd::print(&buf[i]);
}

int main() {
  Board::begin();
  Lcd::begin();

  Lcd::setCursor(0, 0);
  Lcd::print("Hello, IOP I2C!");

  uint32_t seconds = 0;
  Board::run([&]() {
    if (period()) {
      Lcd::setCursor(0, 1);
      Lcd::print("uptime: ");
      printNum(seconds++);
      Lcd::print("s  ");
    }
  });
}
