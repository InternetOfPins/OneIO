#pragma once
#include <stdint.h>
#include <string.h>

namespace oneIO::eeprom {

  // ── Crc32 ─────────────────────────────────────────────────────────────────
  // Standard Ethernet/ZIP CRC-32 (polynomial 0xEDB88320, reflected form).
  // Appended after the data block; checked on load.
  struct Crc32 {
    static constexpr uint16_t size = 4;

    static uint32_t compute(const uint8_t* data, uint16_t len) {
      uint32_t crc = 0xFFFFFFFFu;
      while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++)
          crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
      }
      return ~crc;
    }

    static bool check(const uint8_t* data, uint16_t len, const uint8_t* stored) {
      uint32_t s; memcpy(&s, stored, 4);
      return compute(data, len) == s;
    }

    static void write(uint32_t crc, uint8_t* out) { memcpy(out, &crc, 4); }
  };

  // ── Signature<Magic, Version> ──────────────────────────────────────────────
  // 4-byte block header: [magic_hi][magic_lo][version][seq]
  //   magic   — identifies this application's EEPROM layout
  //   version — increment when Data struct changes; old blocks become invalid
  //   seq     — monotonic write counter for BlockRecycle ordering (uint8_t wrap)
  template<uint16_t Magic = 0xA55A, uint8_t Version = 1>
  struct Signature {
    static constexpr uint16_t size = 4;

    static bool check(const uint8_t* hdr) {
      uint16_t m = (uint16_t(hdr[0]) << 8) | hdr[1];
      return m == Magic && hdr[2] == Version;
    }

    static void write(uint8_t* hdr, uint8_t seq) {
      hdr[0] = uint8_t(Magic >> 8);
      hdr[1] = uint8_t(Magic);
      hdr[2] = Version;
      hdr[3] = seq;
    }

    static uint8_t seq(const uint8_t* hdr) { return hdr[3]; }
  };

  // ── BlockRecycle<ContentSize, AssignedSize, PageSize> ─────────────────────
  // Derives block layout from the three sizing constraints:
  //
  //   ContentSize  = Sig::size + sizeof(Data) + Crc::size  (computed by EepromStore)
  //   blockSize    = smallest multiple of PageSize >= ContentSize
  //   blockCount   = AssignedSize / blockSize
  //
  // On scan: walk all blockCount blocks, select the one with the highest
  //   seq counter (uint8_t, wraps — compared via signed int8_t subtraction).
  // On save: advance current by one (ring), increment seq.
  template<uint16_t ContentSize, uint16_t AssignedSize, uint8_t PageSize = 32>
  struct BlockRecycle {
    static constexpr uint16_t blockSize  =
      ((ContentSize + PageSize - 1u) / PageSize) * PageSize;
    static constexpr uint8_t  blockCount =
      uint8_t(AssignedSize / blockSize);

    static_assert(blockCount >= 2, "AssignedSize too small for even 2 blocks — increase AssignedSize or reduce Data size");

    inline static uint8_t _current = 0;
    inline static uint8_t _seq     = 0;
    inline static bool    _valid   = false;

    static uint8_t current()  { return _current; }
    static uint8_t seq()      { return _seq; }
    static bool    valid()    { return _valid; }

    static void advance() {
      _current = (_current + 1) % blockCount;
      _seq++;
    }

    static void found(uint8_t block, uint8_t s) {
      _current = block; _seq = s; _valid = true;
    }

    // Sequence comparison with uint8_t wrap-around
    static bool seqAfter(uint8_t a, uint8_t b) { return int8_t(a - b) > 0; }
  };

  // ── EepromStore<Eeprom, Data, Sig, Crc, Recycle, BaseAddr> ───────────────
  // Composable EEPROM block store.
  //
  // Block layout per cell (Recycle::blockSize bytes total):
  //   [ Sig::size  header ][ sizeof(Data) ][ Crc::size ][ padding to page boundary ]
  //
  // Recycle::blockCount cells start at BaseAddr.
  //
  // begin() — scans all cells, selects the newest valid one via seq counter.
  // load()  — reads Data from current cell; returns false if no valid cell exists.
  // save()  — advances to the next cell (ring), writes header + data + crc.
  // valid() — true if begin() found at least one valid cell.
  //
  // Eeprom must provide static:
  //   begin()
  //   read (uint16_t addr, uint8_t* buf,       uint16_t len)
  //   write(uint16_t addr, const uint8_t* buf, uint16_t len)
  template<typename Eeprom,
           typename Data,
           typename Sig     = Signature<>,
           typename Crc     = Crc32,
           typename Recycle = BlockRecycle<Sig::size + sizeof(Data) + Crc::size, 256>,
           uint16_t BaseAddr = 0>
  struct EepromStore {
    static constexpr uint16_t dataSize    = sizeof(Data);
    static constexpr uint16_t contentSize = Sig::size + dataSize + Crc::size;

    static_assert(contentSize <= Recycle::blockSize,
      "BlockRecycle blockSize < contentSize — mismatched Recycle type");

    static void begin() { Eeprom::begin(); _scan(); }

    // Returns true if a valid cell was found and data loaded
    static bool load(Data& d) {
      if (!Recycle::valid()) return false;
      uint16_t addr = BaseAddr
                    + uint16_t(Recycle::current()) * Recycle::blockSize
                    + Sig::size;
      Eeprom::read(addr, reinterpret_cast<uint8_t*>(&d), dataSize);
      return true;
    }

    // Advances to next cell (wear leveling), writes header + data + crc
    static void save(const Data& d) {
      Recycle::advance();
      uint16_t addr = BaseAddr + uint16_t(Recycle::current()) * Recycle::blockSize;

      uint8_t hdr[Sig::size];
      Sig::write(hdr, Recycle::seq());
      Eeprom::write(addr, hdr, Sig::size);

      Eeprom::write(addr + Sig::size,
                    reinterpret_cast<const uint8_t*>(&d), dataSize);

      uint32_t crc = Crc::compute(reinterpret_cast<const uint8_t*>(&d), dataSize);
      uint8_t  crcBuf[Crc::size];
      Crc::write(crc, crcBuf);
      Eeprom::write(addr + Sig::size + dataSize, crcBuf, Crc::size);
    }

    static bool valid() { return Recycle::valid(); }

  private:
    static void _scan() {
      int8_t  best    = -1;
      uint8_t bestSeq = 0;

      uint8_t hdr   [Sig::size ];
      uint8_t buf   [dataSize  ];
      uint8_t crcBuf[Crc::size ];

      for (uint8_t i = 0; i < Recycle::blockCount; i++) {
        uint16_t addr = BaseAddr + uint16_t(i) * Recycle::blockSize;

        Eeprom::read(addr, hdr, Sig::size);
        if (!Sig::check(hdr)) continue;

        Eeprom::read(addr + Sig::size,            buf,    dataSize );
        Eeprom::read(addr + Sig::size + dataSize, crcBuf, Crc::size);
        if (!Crc::check(buf, dataSize, crcBuf)) continue;

        uint8_t s = Sig::seq(hdr);
        if (best < 0 || Recycle::seqAfter(s, bestSeq)) {
          best    = int8_t(i);
          bestSeq = s;
        }
      }

      if (best >= 0) Recycle::found(uint8_t(best), bestSeq);
    }
  };

  // ── EepromBlock<Eeprom, Data, AssignedSize, PageSize, BaseAddr, Magic, Version>
  // Convenience alias: wires Signature + Crc32 + BlockRecycle together.
  // BlockRecycle derives blockSize and blockCount from the sizing parameters.
  template<typename Eeprom,
           typename Data,
           uint16_t AssignedSize,
           uint8_t  PageSize   = 32,
           uint16_t BaseAddr   = 0,
           uint16_t Magic      = 0xA55A,
           uint8_t  Version    = 1>
  using EepromBlock = EepromStore<
    Eeprom, Data,
    Signature<Magic, Version>,
    Crc32,
    BlockRecycle<Signature<Magic,Version>::size + sizeof(Data) + Crc32::size,
                 AssignedSize, PageSize>,
    BaseAddr>;

  // ── EepromRoot<ChipBase> ──────────────────────────────────────────────────
  // Chain terminal that seeds eepromBase for component-level address allocation.
  //
  // Components read O::eepromBase for their EepromBlock BaseAddr, then expose
  // eepromBase = O::eepromBase + AssignedSize so the next outer component picks
  // up where this one ends.  The chain computes the full address map at compile
  // time with no manual offset arithmetic.
  //
  // Usage pattern inside a component's Part<O>:
  //
  //   template<typename O>
  //   struct Part : O {
  //     using Store = EepromBlock<Eep, MyData, 128, 32, O::eepromBase>;
  //     static constexpr uint16_t eepromBase = O::eepromBase + 128;
  //     // ... component methods using Store::load() / Store::save() ...
  //   };
  //
  // Composing (innermost component first, outermost last):
  //
  //   using MyDev = hapi::APIOf<EepromRoot<>, SharedComp, SpecificComp>;
  //   // SharedComp  at 0x000 + its AssignedSize
  //   // SpecificComp at SharedComp::eepromBase + its AssignedSize
  //
  // ChipBase offsets the whole allocation within the chip (default 0).
  template<uint16_t ChipBase = 0>
  struct EepromRoot {
    static constexpr uint16_t eepromBase = ChipBase;
  };

} // oneIO::eeprom
