#pragma once
#include <stdint.h>
#include <string.h>

namespace oneIO::eeprom {

  // ── CRC32 ─────────────────────────────────────────────────────────────────
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
  //   version — incremented when Data struct changes (old blocks become invalid)
  //   seq     — monotonic write counter for BlockRecycle ordering (uint8_t, wraps)
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

  // ── BlockRecycle<N> ────────────────────────────────────────────────────────
  // Tracks N blocks in a ring; advances write pointer on each save().
  // On begin(), EepromStore::_scan() calls found() for the newest valid block.
  //
  // Sequence comparison uses signed int8_t subtraction to handle uint8_t wrap.
  // At N=4 this spreads 100 000 guaranteed EEPROM cycles to ~400 000 writes.
  template<uint8_t N = 4>
  struct BlockRecycle {
    static constexpr uint8_t count = N;

    inline static uint8_t _current = 0;
    inline static uint8_t _seq     = 0;
    inline static bool    _valid   = false;

    static uint8_t current() { return _current; }
    static uint8_t seq()     { return _seq; }
    static bool    valid()   { return _valid; }

    static void advance() { _current = (_current + 1) % N; _seq++; }

    static void found(uint8_t block, uint8_t seq) {
      _current = block; _seq = seq; _valid = true;
    }

    // True if sequence a is strictly after b (handles uint8_t wrap-around)
    static bool seqAfter(uint8_t a, uint8_t b) { return int8_t(a - b) > 0; }
  };

  // ── EepromStore<Eeprom, Data, Sig, Crc, Recycle> ─────────────────────────
  // Composable EEPROM block store.
  //
  // Block layout per cell:
  //   [ Sig::size bytes header ][ sizeof(Data) bytes ][ Crc::size bytes ]
  //
  // Recycle::count cells are stored at consecutive addresses from offset 0.
  //
  // begin() — scans all cells, selects the newest valid one.
  // load()  — reads data from current cell; returns false if no valid cell.
  // save()  — advances to next cell, writes header + data + crc.
  // valid() — true after begin() found at least one valid cell.
  //
  // Eeprom must provide static:
  //   begin()
  //   read(uint16_t addr, uint8_t* buf, uint16_t len)
  //   write(uint16_t addr, const uint8_t* buf, uint16_t len)
  template<typename Eeprom,
           typename Data,
           typename Sig     = Signature<>,
           typename Crc     = Crc32,
           typename Recycle = BlockRecycle<4>>
  struct EepromStore {
    static constexpr uint16_t dataSize  = sizeof(Data);
    static constexpr uint16_t blockSize = Sig::size + dataSize + Crc::size;

    static void begin() { Eeprom::begin(); _scan(); }

    static bool load(Data& d) {
      if (!Recycle::valid()) return false;
      uint16_t addr = uint16_t(Recycle::current()) * blockSize + Sig::size;
      Eeprom::read(addr, reinterpret_cast<uint8_t*>(&d), dataSize);
      return true;
    }

    static void save(const Data& d) {
      Recycle::advance();
      uint16_t addr = uint16_t(Recycle::current()) * blockSize;

      uint8_t hdr[Sig::size];
      Sig::write(hdr, Recycle::seq());
      Eeprom::write(addr, hdr, Sig::size);

      Eeprom::write(addr + Sig::size,
                    reinterpret_cast<const uint8_t*>(&d), dataSize);

      uint32_t crc = Crc::compute(reinterpret_cast<const uint8_t*>(&d), dataSize);
      uint8_t crcBuf[Crc::size];
      Crc::write(crc, crcBuf);
      Eeprom::write(addr + Sig::size + dataSize, crcBuf, Crc::size);
    }

    static bool valid() { return Recycle::valid(); }

  private:
    static void _scan() {
      int8_t  best    = -1;
      uint8_t bestSeq = 0;

      uint8_t hdr  [Sig::size ];
      uint8_t buf  [dataSize  ];
      uint8_t crcBuf[Crc::size];

      for (uint8_t i = 0; i < Recycle::count; i++) {
        uint16_t addr = uint16_t(i) * blockSize;

        Eeprom::read(addr, hdr, Sig::size);
        if (!Sig::check(hdr)) continue;

        Eeprom::read(addr + Sig::size,            buf,    dataSize );
        Eeprom::read(addr + Sig::size + dataSize, crcBuf, Crc::size);
        if (!Crc::check(buf, dataSize, crcBuf)) continue;

        uint8_t seq = Sig::seq(hdr);
        if (best < 0 || Recycle::seqAfter(seq, bestSeq)) {
          best    = int8_t(i);
          bestSeq = seq;
        }
      }

      if (best >= 0) Recycle::found(uint8_t(best), bestSeq);
    }
  };

  // ── Convenience default ───────────────────────────────────────────────────
  // EepromBlock<Eeprom, Data, Magic, Version, N> — Signature + Crc32 + BlockRecycle<N>
  template<typename Eeprom, typename Data,
           uint16_t Magic   = 0xA55A,
           uint8_t  Version = 1,
           uint8_t  N       = 4>
  using EepromBlock = EepromStore<Eeprom, Data,
                                  Signature<Magic, Version>,
                                  Crc32,
                                  BlockRecycle<N>>;

} // oneIO::eeprom
