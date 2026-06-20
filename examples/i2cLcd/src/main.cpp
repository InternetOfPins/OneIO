#include <chips/avr/avrDevice.h>
#include <oneIO/display/i2cLcd.h>

using namespace onePin;
using namespace oneBit;
using namespace hw::avr;
using namespace oneIO::display;

// PCF8574 I2C LCD backpack — address 0x27 (A0-A2 = 000).
// SDA = A4 (Uno/Nano), SCL = A5. No extra wiring needed.
using Twi = chip::TwiMaster<>;            // 100 kHz, 16 MHz CPU
using Lcd = I2cLcd<Twi, 0x27>;

using SysTick = chip::SysTick0<>;
using Board   = AVR::Board<Boot<SysTick>>;

#ifdef IOP
IOP_TIMER0_ISR(Board)
#endif

SysTick::Period<1000> period;

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
