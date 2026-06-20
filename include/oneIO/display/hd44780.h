#pragma once
#include <hapi/hapi.h>
#include <stdint.h>

#ifdef __AVR__
  #include <util/delay.h>
  #define LCD_DELAY_US(us)  _delay_us(us)
  #define LCD_DELAY_MS(ms)  _delay_ms(ms)
#else
  static inline void _lcd_delay_us(uint32_t us) {
    for (volatile uint32_t i = us * 72; i; --i);
  }
  static inline void _lcd_delay_ms(uint32_t ms) {
    while (ms--) _lcd_delay_us(1000);
  }
  #define LCD_DELAY_US(us)  _lcd_delay_us(us)
  #define LCD_DELAY_MS(ms)  _lcd_delay_ms(ms)
#endif

namespace oneIO::display {

  struct LcdDef {
    LcdDef() = delete;
    static void begin() {}
  };

  // HD44780 4-bit driver. RS, EN, D4..D7 are fully-configured IOP OutPin types.
  // RW tied to GND (write-only — saves a pin, standard practice).
  template<typename RS, typename EN, typename D4, typename D5, typename D6, typename D7>
  struct Hd44780 {
    template<typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;

    private:
      static void pulse_en() {
        EN::on();
        LCD_DELAY_US(1);
        EN::off();
        LCD_DELAY_US(50);
      }

      static void send_nibble(uint8_t n) {
        (n & 0x1) ? D4::on() : D4::off();
        (n & 0x2) ? D5::on() : D5::off();
        (n & 0x4) ? D6::on() : D6::off();
        (n & 0x8) ? D7::on() : D7::off();
        pulse_en();
      }

      static void send_byte(bool rs, uint8_t b) {
        rs ? RS::on() : RS::off();
        send_nibble(b >> 4);
        send_nibble(b & 0xF);
        LCD_DELAY_US(50);
      }

    public:
      static void command(uint8_t cmd) { send_byte(false, cmd); }
      static void data(uint8_t d)      { send_byte(true,  d);   }

      static void begin() {
        RS::begin(); EN::begin();
        D4::begin(); D5::begin(); D6::begin(); D7::begin();

        RS::off(); EN::off();
        D4::off(); D5::off(); D6::off(); D7::off();

        LCD_DELAY_MS(50);      // >40 ms after Vcc rise

        // 8-bit mode recovery × 3
        RS::off();
        send_nibble(0x3); LCD_DELAY_MS(5);
        send_nibble(0x3); LCD_DELAY_US(150);
        send_nibble(0x3); LCD_DELAY_US(50);

        // Switch to 4-bit mode
        send_nibble(0x2); LCD_DELAY_US(50);

        command(0x28);   // function set: 4-bit, 2 lines, 5×8
        command(0x0C);   // display on, cursor off, blink off
        command(0x01);   // clear display
        LCD_DELAY_MS(2);
        command(0x06);   // entry mode: increment, no shift

        Base::begin();
      }

      static void clear() { command(0x01); LCD_DELAY_MS(2); }
      static void home()  { command(0x02); LCD_DELAY_MS(2); }

      static void setCursor(uint8_t col, uint8_t row) {
        static constexpr uint8_t row_addr[] = {0x00, 0x40, 0x14, 0x54};
        command(0x80 | (col + row_addr[row & 0x3]));
      }

      static void print(char c)        { data((uint8_t)c); }
      static void print(const char* s) { while (*s) print(*s++); }
      static void println(uint8_t row, const char* s) {
        setCursor(0, row); print(s);
      }
    };
  };

} // oneIO::display
