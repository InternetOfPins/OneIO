// Compatibility shim — ArduinoWire<> now lives in oneBus::ArduinoWire<>.
// Include <oneBus/arduinoI2C.h> directly in new code.
#pragma once
#include <oneBus/arduinoI2C.h>
#ifdef ARDUINO
namespace oneIO::display {
  using oneBus::ArduinoWire;
} // oneIO::display
#endif
