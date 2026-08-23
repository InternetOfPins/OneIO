/*
 * HAPI-composed Arduino_GFX adapter, hosted in OneIO -- see README.md in this
 * directory for scope and rationale (moved here from the Arduino_GFX fork's
 * own src/hapi/, to avoid a real include-path collision with IOP's own
 * HAPI library (both name a top-level hapi/ folder).
 *
 * Compile-time-parameterized stand-in for src/databus/Arduino_Wire.{h,cpp}
 * (the I2C/TwoWire databus). I2CAddr/CommandPrefix/DataPrefix become
 * non-type template parameters instead of runtime constructor arguments --
 * no per-instance _i2c_addr/_command_prefix/_data_prefix fields, no vtable
 * (no Arduino_DataBus base). `wire` stays a runtime `TwoWire*` constructor
 * argument: unlike a GPIO pin number, the Wire instance is an external
 * singleton object (`Wire`, `Wire1`, ...) with its own runtime state, not a
 * compile-time constant a template parameter could usefully carry.
 */
#pragma once

#include <Arduino_DataBus.h>
#include <Wire.h>

#ifndef TWI_BUFFER_LENGTH
#if defined(I2C_BUFFER_LENGTH)
#define TWI_BUFFER_LENGTH I2C_BUFFER_LENGTH
#else
#define TWI_BUFFER_LENGTH 32
#endif
#endif

namespace hapi_gfx
{

  template <uint8_t I2CAddr, int8_t CommandPrefix, int8_t DataPrefix>
  class Arduino_Wire_HAPI
  {
  public:
    Arduino_Wire_HAPI(TwoWire *wire = &Wire) : _wire(wire) {}

    bool begin(int32_t speed = GFX_NOT_DEFINED, int8_t = GFX_NOT_DEFINED)
    {
      if (speed != GFX_NOT_DEFINED)
      {
        _speed = speed;
        _wire->setClock(_speed);
      }

      _wire->begin();

      _wire->beginTransmission(I2CAddr);
      return _wire->endTransmission() == 0;
    }

    void beginWrite()
    {
      if (_speed != GFX_NOT_DEFINED)
      {
        _wire->setClock(_speed);
      }
      _wire->beginTransmission(I2CAddr);
    }

    void endWrite() { _wire->endTransmission(); }

    void write(uint8_t d) { _wire->write(d); }

    void writeCommand(uint8_t c)
    {
      _wire->write(uint8_t(CommandPrefix));
      _wire->write(c);
    }

    void writeCommand16(uint16_t) { /* not implemented -- matches Arduino_Wire */ }
    void write16(uint16_t) { /* not implemented -- matches Arduino_Wire */ }
    void writeRepeat(uint16_t, uint32_t) { /* not implemented -- matches Arduino_Wire */ }

    // Arduino_DataBus provides these as non-virtual defaults on the base
    // class (writeCommand(c); write(d); etc. -- see Arduino_DataBus.cpp);
    // with no base class here, they're spelled out directly instead.
    void writeC8D8(uint8_t c, uint8_t d) { writeCommand(c); write(d); }
    void writeC16D16(uint16_t c, uint16_t d) { writeCommand16(c); write16(d); }
    void writeC8D16(uint8_t c, uint16_t d) { writeCommand(c); write16(d); }
    void writeC8D16D16(uint8_t c, uint16_t d1, uint16_t d2) { writeCommand(c); write16(d1); write16(d2); }
    void writeC8D16D16Split(uint8_t c, uint16_t d1, uint16_t d2) { writeC8D16D16(c, d1, d2); }

    void writeCommandBytes(uint8_t *data, uint32_t len)
    {
      uint16_t bytesOut = 1;
      _wire->write(uint8_t(CommandPrefix));

      while (len--)
      {
        if (bytesOut >= TWI_BUFFER_LENGTH)
        {
          _wire->endTransmission();
          _wire->beginTransmission(I2CAddr);
          _wire->write(uint8_t(CommandPrefix));
          bytesOut = 1;
        }
        _wire->write(*data++);
        bytesOut++;
      }
    }

    // write data to the bus using the data prefix; splits into TWI_BUFFER_LENGTH
    // chunks the same way Arduino_Wire::writeBytes does.
    void writeBytes(uint8_t *data, uint32_t len)
    {
      uint16_t bytesOut = 1;
      _wire->write(uint8_t(DataPrefix));

      while (len--)
      {
        if (bytesOut >= TWI_BUFFER_LENGTH)
        {
          _wire->endTransmission();
          _wire->beginTransmission(I2CAddr);
          _wire->write(uint8_t(DataPrefix));
          bytesOut = 1;
        }
        _wire->write(*data++);
        bytesOut++;
      }
    }

    void writePixels(uint16_t *, uint32_t) { /* not implemented -- matches Arduino_Wire */ }

  private:
    TwoWire *_wire;
    int32_t _speed = GFX_NOT_DEFINED;
  };

} // namespace hapi_gfx
