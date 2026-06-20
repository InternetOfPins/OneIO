#include <chips/avr/avrDevice.h>
#include <oneIO/display/hd44780.h>

using namespace onePin;
using namespace oneBit;
using namespace hw::avr;
using namespace oneIO::display;

// Classic Arduino tutorial wiring (Uno):
//   RS=D12 (PB4)  EN=D11 (PB3)
//   D4=D5  (PD5)  D5=D4  (PD4)  D6=D3  (PD3)  D7=D2  (PD2)
using PinRS = AVR::OutPin<Pins<4>, chip::PortB>;
using PinEN = AVR::OutPin<Pins<3>, chip::PortB>;
using PinD4 = AVR::OutPin<Pins<5>, chip::PortD>;
using PinD5 = AVR::OutPin<Pins<4>, chip::PortD>;
using PinD6 = AVR::OutPin<Pins<3>, chip::PortD>;
using PinD7 = AVR::OutPin<Pins<2>, chip::PortD>;

using SysTick = chip::SysTick0<>;
using Lcd     = hapi::APIOf<LcdDef, Hd44780<PinRS, PinEN, PinD4, PinD5, PinD6, PinD7>>;
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
  Lcd::print("Hello, IOP!");

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
