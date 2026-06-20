#include <chips/avr/avrDevice.h>
#include <oneIO/display/i2cOled.h>

using namespace onePin;
using namespace oneBit;
using namespace hw::avr;
using namespace oneIO::display;

// SSD1306 128×64 I2C OLED — SDA=A4, SCL=A5, address 0x3C.
using Twi  = chip::TwiMaster<>;
using Oled = I2cOled<Twi, 0x3C>;

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
  if (!n) { Oled::print('0'); return; }
  while (n && i) { buf[--i] = '0' + (n % 10); n /= 10; }
  Oled::print(&buf[i]);
}

int main() {
  Board::begin();
  Oled::begin();

  Oled::setCursor(0, 0); Oled::print("IOP SSD1306");
  Oled::setCursor(0, 1); Oled::print("128x64 OLED");
  Oled::setCursor(0, 2); Oled::print("I2C 0x3C");
  Oled::setCursor(0, 3); Oled::print("---");

  uint32_t seconds = 0;
  Board::run([&]() {
    if (period()) {
      Oled::setCursor(0, 5);
      Oled::print("uptime: ");
      printNum(seconds++);
      Oled::print("s   ");
    }
  });
}
