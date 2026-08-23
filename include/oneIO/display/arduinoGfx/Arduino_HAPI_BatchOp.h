/*
 * HAPI-composed Arduino_GFX adapter, hosted in OneIO -- see README.md in this
 * directory for scope and rationale (moved here from the Arduino_GFX fork's
 * own src/hapi/, to avoid a real include-path collision with IOP's own
 * HAPI library (both name a top-level hapi/ folder).
 *
 * Templated stand-in for Arduino_DataBus::batchOperation() (src/Arduino_DataBus.cpp).
 * Interprets the exact same spi_operation_type_t byte-code arrays already shipped
 * in this repo (e.g. st7789_type1_init_operations in src/display/Arduino_ST7789.h)
 * so display init sequences stay 100% reused, byte-for-byte -- only the dispatch
 * mechanism changes: `Bus` is a concrete type known at compile time, so every
 * bus.writeCommand()/write() call below is direct and inlinable, not a vtable hop.
 */
#pragma once

#include <Arduino_DataBus.h>

namespace hapi_gfx
{

  template <typename Bus>
  inline void batchOperation(Bus &bus, const uint8_t *operations, size_t len)
  {
    for (size_t i = 0; i < len; ++i)
    {
      uint8_t l = 0;
      switch (operations[i])
      {
      case BEGIN_WRITE:
        bus.beginWrite();
        break;
      case WRITE_C8_D16:
        l++;
        /* fall through */
      case WRITE_C8_D8:
        l++;
        /* fall through */
      case WRITE_COMMAND_8:
        bus.writeCommand(operations[++i]);
        break;
      case WRITE_C16_D16:
        l = 2;
        /* fall through */
      case WRITE_COMMAND_16:
      {
        uint8_t msb = operations[++i];
        uint8_t lsb = operations[++i];
        bus.writeCommand16((uint16_t(msb) << 8) | lsb);
        break;
      }
      case WRITE_COMMAND_BYTES:
        l = operations[++i];
        bus.writeCommandBytes((uint8_t *)(operations + i + 1), l);
        i += l;
        l = 0;
        break;
      case WRITE_DATA_8:
        l = 1;
        break;
      case WRITE_DATA_16:
        l = 2;
        break;
      case WRITE_BYTES:
        l = operations[++i];
        break;
      case WRITE_C8_BYTES:
        bus.writeCommand(operations[++i]);
        l = operations[++i];
        break;
      case END_WRITE:
        bus.endWrite();
        break;
      case DELAY:
        delay(operations[++i]);
        break;
      default:
        break;
      }
      while (l--)
      {
        bus.write(operations[++i]);
      }
    }
  }

} // namespace hapi_gfx
