/**
 * @file ds18b20.h
 * @author Rui Azevedo (ruihfazevedo@gmail.com)
 * @brief DS18B20 1-Wire temperature sensor HAPI component.
 */

#pragma once
#include <hapi/hapi.h>

namespace oneSensor {

  /// @brief Dallas DS18B20 1-Wire temperature sensor; convert() + read() returns °C as fixed-point
  template<typename Bus>
  struct DS18B20 {
    struct Part {
      static bool    start();           // reset + presence pulse
      static void    convert();         // start temperature conversion
      static int16_t read();            // read raw value (1/16 °C units)
      static float   celsius();         // convert raw to °C
    };
  };

} // oneSensor
