#pragma once
#include <hapi/hapi.h>
#include <stdint.h>
#ifdef ARDUINO
#include <SD.h>

namespace oneIO::storage {

  // SDCard<CsPin> — Arduino SD library wrapper for IOP.
  //
  // Provides raw byte-level read/write at absolute file offsets,
  // matching the AT24C driver interface so EepromStore<> can use
  // either EEPROM or SD as its backing store.
  //
  // Each read/write opens+seeks+closes the file — safe for infrequent
  // config saves. For high-frequency logging, extend with a held-open File.
  //
  // File layout mirrors the flat address space of AT24C:
  //   read(addr, buf, len)  — reads len bytes at byte offset addr
  //   write(addr, buf, len) — writes len bytes at byte offset addr
  //   No wear leveling here — BlockRecycle handles that above this layer.
  //
  // CsPin: SPI chip-select pin for the SD module.
  // Filename: 8.3 format (FAT16/FAT32 constraint on most SD libs).

  template<uint8_t CsPin, const char* Filename>
  struct SDCard {
    struct StoreDef { StoreDef() = delete; };

    template<typename O>
    struct Part : O {
      inline static bool _ready = false;

      static bool begin() {
        _ready = SD.begin(CsPin);
        O::begin();
        return _ready;
      }

      static bool ready() { return _ready; }

      static void read(uint32_t addr, uint8_t* buf, uint16_t len) {
        File f = SD.open(Filename, FILE_READ);
        if (!f) return;
        f.seek(addr);
        f.read(buf, len);
        f.close();
      }

      static void write(uint32_t addr, const uint8_t* buf, uint16_t len) {
        // SD.h FILE_WRITE opens at end; we need random-access write.
        // Workaround: open for write (creates if absent), seek, write.
        File f = SD.open(Filename, FILE_WRITE);
        if (!f) return;
        f.seek(addr);
        f.write(buf, len);
        f.close();
      }

      // Erase assigned region (fill with 0xFF to match EEPROM blank state)
      static void erase(uint32_t addr, uint16_t len) {
        File f = SD.open(Filename, FILE_WRITE);
        if (!f) return;
        f.seek(addr);
        uint8_t blank = 0xFF;
        for (uint16_t i = 0; i < len; i++) f.write(&blank, 1);
        f.close();
      }

      static bool exists() { return SD.exists(Filename); }
      static void remove() { SD.remove(Filename); }
    };

    using Api = hapi::APIOf<StoreDef, SDCard<CsPin, Filename>>;
  };

  // SdRoot<BaseAddr> — HAPI chain terminal for SD address allocation.
  // Same contract as EepromRoot<> but for SD file offsets.
  template<uint32_t BaseAddr = 0>
  struct SdRoot {
    static constexpr uint32_t sdBase = BaseAddr;
  };

} // oneIO::storage
#endif
