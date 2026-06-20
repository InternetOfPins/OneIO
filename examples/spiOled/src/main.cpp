#include <chips/avr/avrDevice.h>
#include <oneIO/display/spiOled.h>

using namespace onePin;
using namespace oneBit;
using namespace hw::avr;
using namespace oneIO::display;

// SSD1306 128×64 SPI OLED.
// Hardware SPI pins (fixed): MOSI=D11, SCK=D13.
// User pins: CS=D10(PB2), DC=D9(PB1), RST=D8(PB0).
using CS  = AVR::OutPin<Pins<PB2>, chip::PortB>;
using DC  = AVR::OutPin<Pins<PB1>, chip::PortB>;
using RST = AVR::OutPin<Pins<PB0>, chip::PortB>;

using Spi  = chip::SpiMaster;
using Oled = SpiOled<Spi, CS, DC, RST>;

using SysTick = chip::SysTick0<>;
using Board   = AVR::Board<Boot<SysTick>>;

#ifdef IOP
IOP_TIMER0_ISR(Board)
#endif

SysTick::Period<1000> period;

static void printNum(uint32_t n) {
  char buf[11]; uint8_t i = 10; buf[10] = '\0';
  if (!n) { Oled::print('0'); return; }
  while (n && i) { buf[--i] = '0' + (n % 10); n /= 10; }
  Oled::print(&buf[i]);
}

int main() {
  Board::begin();
  Oled::begin();

  Oled::setCursor(0, 0); Oled::print("IOP SSD1306");
  Oled::setCursor(0, 1); Oled::print("SPI mode");
  Oled::setCursor(0, 2); Oled::print("128x64 OLED");

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
