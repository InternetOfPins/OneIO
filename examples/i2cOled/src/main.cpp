#include <oneIO/display/i2cOled.h>
using namespace oneIO::display;

#if defined(ARDUINO_ARCH_ESP32)
  #include <chips/esp32/esp32Device.h>
  using namespace hw::esp32;
  using Twi   = chip::TwiMaster<21, 22>;   // SDA=GPIO21, SCL=GPIO22
  using Clock = chip::SysClock;
  using Board = Esp32Dev::Board<onePin::Boot<Clock>>;
#else
  #include <chips/avr/avrDevice.h>
  using namespace onePin;
  using namespace oneBit;
  using namespace hw::avr;
  using Twi   = chip::TwiMaster<>;
  using Clock = chip::SysTick0<>;
  using Board = AVR::Board<Boot<Clock>>;
  #ifdef IOP
    IOP_TIMER0_ISR(Board)
  #endif
#endif

using Oled = I2cOled<Twi, 0x3C>;

Clock::Period<1000> period;

static void printNum(uint32_t n) {
  char buf[11]; uint8_t i = 10; buf[10] = '\0';
  if (!n) { Oled::print('0'); return; }
  while (n && i) { buf[--i] = '0' + (n % 10); n /= 10; }
  Oled::print(&buf[i]);
}

#ifdef ARDUINO
void setup() {
  Board::begin();
  Oled::begin();
  Oled::setCursor(0, 0); Oled::print("IOP SSD1306");
  Oled::setCursor(0, 1); Oled::print("I2C 0x3C");

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
void loop() {}

#else

int main() {
  Board::begin();
  Oled::begin();
  Oled::setCursor(0, 0); Oled::print("IOP SSD1306");
  Oled::setCursor(0, 1); Oled::print("128x64 OLED");
  Oled::setCursor(0, 2); Oled::print("I2C 0x3C");

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
#endif
