#pragma once
#include <hapi/hapi.h>
#include <stdint.h>

namespace oneIO::sensor {

  // MAX31855 thermocouple amplifier — SPI read-only, 32-bit frame.
  //
  // Frame layout (MSB first):
  //   [31:18] Thermocouple temp — 14-bit signed, 0.25 °C/LSB
  //   [17]    reserved
  //   [16]    Fault — 1 if any fault bit set
  //   [15:4]  Cold-junction temp — 12-bit signed, 0.0625 °C/LSB
  //   [3]     reserved
  //   [2]     SCV — short to VCC
  //   [1]     SCG — short to GND
  //   [0]     OC  — open circuit
  //
  // SpiMaster must provide: begin(), transfer(uint8_t) → uint8_t.
  // CsPin must provide: begin(), on(), off().
  /// @brief MAX31855 SPI thermocouple amplifier; read() returns temp_c + fault bits
  template<typename SpiMaster, typename CsPin>
  struct MAX31855 {
    struct SensorDef { SensorDef() = delete; };

    struct Reading {
      int16_t tc_raw;   // 14-bit signed thermocouple ticks (0.25 °C each)
      int16_t cj_raw;   // 12-bit signed cold-junction ticks (0.0625 °C each)
      uint8_t fault;    // bit0=OC, bit1=SCG, bit2=SCV
      bool    ok;       // false if any fault set

      float tempC()   const { return tc_raw * 0.25f; }
      float coldC()   const { return cj_raw * 0.0625f; }
      bool  openCircuit() const { return fault & 0x01; }
      bool  shortGnd()    const { return fault & 0x02; }
      bool  shortVcc()    const { return fault & 0x04; }
    };

    using Api = hapi::APIOf<SensorDef, MAX31855<SpiMaster, CsPin>>;

    template<typename O>
    struct Part : O {
      static void begin() { CsPin::begin(); CsPin::on(); SpiMaster::begin(); O::begin(); }

      static Reading read() {
        CsPin::off();
        // Receive 4 bytes — send 0xFF (dummy) while clocking in data
        uint8_t b0 = SpiMaster::transfer(0xFF);
        uint8_t b1 = SpiMaster::transfer(0xFF);
        uint8_t b2 = SpiMaster::transfer(0xFF);
        uint8_t b3 = SpiMaster::transfer(0xFF);
        CsPin::on();

        uint32_t raw = (uint32_t(b0)<<24)|(uint32_t(b1)<<16)|(uint32_t(b2)<<8)|b3;

        // Thermocouple: bits 31-18 (14-bit signed, sign at bit 31)
        uint16_t tc_u = uint16_t(raw >> 18);
        int16_t  tc   = (tc_u & 0x2000) ? int16_t(tc_u | 0xC000) : int16_t(tc_u);

        // Cold junction: bits 15-4 (12-bit signed, sign at bit 15)
        uint16_t cj_u = uint16_t((raw >> 4) & 0x0FFF);
        int16_t  cj   = (cj_u & 0x0800) ? int16_t(cj_u | 0xF000) : int16_t(cj_u);

        bool fault = (raw >> 16) & 1;
        return { tc, cj, uint8_t(raw & 0x07), !fault };
      }
    };
  };

} // oneIO::sensor
