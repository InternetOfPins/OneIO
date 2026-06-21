#pragma once
#include <hapi/hapi.h>
#include <stdint.h>
#ifdef ARDUINO
#include <oneIO/display/arduinoWire.h>

namespace oneIO::pwm {

  // PCA9685 — I2C 16-channel 12-bit PWM controller (NXP).
  //
  // I2C address: 0x40..0x7F (6 address bits A0..A5, base 0x40).
  // All 16 channels are independent; each has a 12-bit ON and OFF counter
  // (0..4095) relative to the 4096-step PWM cycle.
  //
  // F_PWM = F_OSC / ((prescale + 1) * 4096)   where F_OSC = 25 MHz internal
  //   prescale = round(F_OSC / (4096 * F_PWM)) - 1
  //   Common values:  50 Hz → 121,  60 Hz → 100,  200 Hz → 29,  1 kHz → 5
  //
  // Duty cycle: OFF = duty (0..4095), ON = 0  →  duty/4096 × 100%
  // Full ON:  ON_H bit 4 set → ignores OFF value
  // Full OFF: OFF_H bit 4 set → ignores ON value

  namespace detail {
    static constexpr uint8_t PCA_MODE1      = 0x00;
    static constexpr uint8_t PCA_MODE2      = 0x01;
    static constexpr uint8_t PCA_LED0_ON_L  = 0x06;
    static constexpr uint8_t PCA_ALL_ON_L   = 0xFA;
    static constexpr uint8_t PCA_PRESCALE   = 0xFE;
    static constexpr uint8_t PCA_AI         = (1<<5);  // auto-increment
    static constexpr uint8_t PCA_SLEEP      = (1<<4);
    static constexpr uint8_t PCA_ALLCALL    = (1<<0);
    static constexpr uint8_t PCA_OUTDRV     = (1<<2);  // totem-pole outputs
    static constexpr uint8_t PCA_FULL_ON    = (1<<4);
    static constexpr uint8_t PCA_FULL_OFF   = (1<<4);
  }

  template<typename TwiMaster, uint8_t Addr = 0x40>
  struct PCA9685 {
    struct PwmDef { PwmDef() = delete; };

    template<typename O>
    struct Part : O {
      static void begin(uint16_t freq_hz = 1000) {
        TwiMaster::begin();
        _write_reg(detail::PCA_MODE1, detail::PCA_ALLCALL);
        delay(1);
        _write_reg(detail::PCA_MODE2, detail::PCA_OUTDRV);
        set_freq(freq_hz);
        O::begin();
      }

      // Set PWM frequency (Hz). Requires sleep mode briefly.
      static void set_freq(uint16_t freq_hz) {
        // prescale = round(25e6 / (4096 * freq)) - 1, clamped 3..255
        uint32_t prescale = (25000000UL + (uint32_t)freq_hz * 2048UL)
                            / ((uint32_t)freq_hz * 4096UL) - 1;
        if (prescale < 3)   prescale = 3;
        if (prescale > 255) prescale = 255;

        uint8_t old = _read_reg(detail::PCA_MODE1);
        _write_reg(detail::PCA_MODE1, (old & 0x7F) | detail::PCA_SLEEP);
        _write_reg(detail::PCA_PRESCALE, uint8_t(prescale));
        _write_reg(detail::PCA_MODE1, old);
        delay(1);
        _write_reg(detail::PCA_MODE1, old | detail::PCA_AI);
      }

      // Set raw ON/OFF counters for channel ch (0..15)
      static void set_raw(uint8_t ch, uint16_t on, uint16_t off) {
        uint8_t reg = detail::PCA_LED0_ON_L + 4 * ch;
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(reg);
        TwiMaster::write_byte(uint8_t(on));
        TwiMaster::write_byte(uint8_t(on >> 8));
        TwiMaster::write_byte(uint8_t(off));
        TwiMaster::write_byte(uint8_t(off >> 8));
        TwiMaster::end_write();
      }

      // Set duty cycle duty=0..4095 (0=off, 4095=full on)
      static void set(uint8_t ch, uint16_t duty) {
        set_raw(ch, 0, duty & 0x0FFF);
      }

      // Convenience: 0..255 duty (rescaled to 12-bit)
      static void set8(uint8_t ch, uint8_t duty8) {
        set(ch, uint16_t(duty8) << 4);
      }

      // Full ON / full OFF (hardware bits, no PWM flicker)
      static void full_on(uint8_t ch) {
        set_raw(ch, uint16_t(detail::PCA_FULL_ON) << 8, 0);
      }
      static void full_off(uint8_t ch) {
        set_raw(ch, 0, uint16_t(detail::PCA_FULL_OFF) << 8);
      }

      // Apply same duty to all 16 channels at once
      static void set_all(uint16_t duty) {
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(detail::PCA_ALL_ON_L);
        TwiMaster::write_byte(0x00);
        TwiMaster::write_byte(0x00);
        TwiMaster::write_byte(uint8_t(duty));
        TwiMaster::write_byte(uint8_t(duty >> 8));
        TwiMaster::end_write();
      }

    private:
      static void _write_reg(uint8_t reg, uint8_t val) {
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(reg);
        TwiMaster::write_byte(val);
        TwiMaster::end_write();
      }
      static uint8_t _read_reg(uint8_t reg) {
        TwiMaster::begin_write(Addr);
        TwiMaster::write_byte(reg);
        TwiMaster::end_write();
        TwiMaster::request_from(Addr, uint8_t(1));
        return TwiMaster::read_byte();
      }
    };

    using Api = hapi::APIOf<PwmDef, PCA9685<TwiMaster, Addr>>;
  };

  template<TwoWire& wire, uint8_t Addr = 0x40, int sda = -1, int scl = -1>
  struct PCA9685Wire : PCA9685<oneIO::display::ArduinoWire<wire, sda, scl>, Addr> {};

} // oneIO::pwm
#endif
