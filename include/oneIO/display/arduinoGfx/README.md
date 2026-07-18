# HAPI-composed Arduino_GFX adapter (proof of concept)

Compile-time-composed alternative to [Arduino_GFX](https://github.com/moononournation/Arduino_GFX)'s
runtime-polymorphic bus/display split (an abstract `Arduino_DataBus` base
with pure-virtual dispatch on every command/pixel write). Hosted here in
OneIO rather than in the Arduino_GFX checkout itself -- see "Why this lives
in OneIO, not the Arduino_GFX fork" below.

**Cross-repo relationship, up front:** this directory reuses register
`#define`s and init-sequence byte arrays from Arduino_GFX's own real headers
(`<display/Arduino_ST7789.h>` etc.) via plain `#include` -- read-only data,
safe to reuse, nothing duplicated. That means a consumer of these files needs
*both* this OneIO checkout *and* a local checkout of
[`moononournation/Arduino_GFX`](https://github.com/moononournation/Arduino_GFX)
staged and resolvable on the include path (`lib_extra_dirs` + a locally
staged `library.json`, same technique documented in
`Arduino_GFX/bench/README.md` and every consumer's own `platformio.ini`
comments -- PlatformIO's `symlink://` `lib_deps` protocol doesn't reliably
resolve Arduino-format libraries, verified the hard way). OneIO's own
`library.json` does **not** declare Arduino_GFX as a hard dependency --
OneIO's other display drivers (`ssd1306.h`, `st7735.h`, etc., all in the
parent `display/` directory) are unrelated, from-scratch implementations
that don't need it. Only consumers that specifically want *this* directory's
Arduino_GFX-flavored classes need to stage Arduino_GFX too.

## What's here

- [`Arduino_ESP32PAR8_HAPI.h`](Arduino_ESP32PAR8_HAPI.h) -- stand-in for
  Arduino_GFX's `src/databus/Arduino_ESP32PAR8.{h,cpp}` (8-bit parallel bus),
  pins as NTTPs.
- [`Arduino_Wire_HAPI.h`](Arduino_Wire_HAPI.h) -- stand-in for
  `src/databus/Arduino_Wire.{h,cpp}` (I2C bus), address/control-bytes as NTTPs.
- [`Arduino_ST7789_HAPI.h`](Arduino_ST7789_HAPI.h) -- stand-in for
  `src/display/Arduino_ST7789.{h,cpp}`, templated on `Bus` instead of holding
  an `Arduino_DataBus*`. Pairs with the parallel bus above.
- [`Arduino_ST7735_HAPI.h`](Arduino_ST7735_HAPI.h) -- second display, same
  pattern, stand-in for `src/display/Arduino_ST7735.{h,cpp}`. Shares ST7789's
  command set; different init sequence and MADCTL/BGR bit mapping, reused
  as-is from the original header.
- [`Arduino_SSD1306_HAPI.h`](Arduino_SSD1306_HAPI.h) -- third display, stand-in
  for `src/display/Arduino_SSD1306.{h,cpp}` (monochrome I2C OLED, page/column
  addressed) -- pairs with `Arduino_Wire_HAPI.h` above, the "at least one
  display driver over I2C" entry, and the one **verified on real hardware**
  (see below). W/H are template parameters here (the original recomputes a
  6-way resolution table from runtime WIDTH/HEIGHT on every `begin()`; with
  W/H compile-time known this collapses to `if constexpr`, one branch's
  worth of code per instantiation instead of all six checked at runtime).
- [`Arduino_HAPI_BatchOp.h`](Arduino_HAPI_BatchOp.h) -- a `Bus`-templated
  replay of `Arduino_DataBus::batchOperation()`, shared by all display ports
  above, not written per-display.
- [`Arduino_GFX_HAPI.h`](Arduino_GFX_HAPI.h) -- CRTP stand-in for Arduino_GFX's
  `src/Arduino_GFX.{h,cpp}`: the ~30 drawing primitives (lines, rects,
  circles, ellipses, arcs, triangles, roundrects, classic-font text via
  `Print`) that the original implements exactly once and every display
  inherits for free. `Arduino_ST7789_HAPI` sits on this now (see
  "PDQgraphicstest" below); `Arduino_ST7735_HAPI`/`Arduino_SSD1306_HAPI`
  don't yet (not needed for either deliverable so far -- straightforward to
  retrofit the same way).
- [`Arduino_IOPSPI_HAPI.h`](Arduino_IOPSPI_HAPI.h) -- SPI bus backed by IOP's
  own existing `hw::esp32::Esp32SpiMaster`/`Esp32OutPin` (OneChip), not a
  from-scratch register-level driver -- see "PDQgraphicstest" below and that
  file's own header comment for why.

Reproducible size/disassembly comparisons against the original library live
in the Arduino_GFX checkout itself, at `Arduino_GFX/bench/` (top-level, a
sibling of `src/`) -- see "Reading the numbers" below.

## Why this lives in OneIO, not the Arduino_GFX fork

These files originally lived at `Arduino_GFX/src/hapi/` (a new, non-invasive
addition to a local Arduino_GFX fork, touching no existing file). That
produced include paths like `#include <hapi/Arduino_ST7789_HAPI.h>` --
because Arduino_GFX is an Arduino-1.5-format library exposing its whole
`src/` as the include root, so `src/hapi/X.h` resolves as `<hapi/X.h>`. That
collided *in form* with IOP's own real `HAPI` library, which also publishes
a top-level `hapi/` include folder (`HAPI/include/hapi/{base,chain,hapi,
meta,rules}.h`) -- a real, demonstrated bug: PlatformIO's dependency finder
silently dropped the Arduino_GFX library from the actual compiler `-I`
flags with no error message when both "HAPI" and "GFX Library for Arduino"
were declared dependencies of the same project (traced to this exact
folder-name collision). Rather than patch around it per-project again,
these classes moved into OneIO -- which already hosts exactly this shape of
thing (third-party/hand-rolled display-driver adapters: `ssd1306.h`,
`u8g2Spi.h`, `ucgSpi.h` in the parent `display/` directory) -- under this
dedicated `arduinoGfx/` subdirectory (so filenames/types here don't collide
with OneIO's own unrelated native `Ssd1306`/`St7735` drivers of the same
chip names one level up). Namespace stayed `hapi_gfx::` rather than moving
under `oneIO::` -- preserves identity, and avoids exactly the kind of
same-name collision (`Ssd1306` vs. `Arduino_SSD1306_HAPI`'s own internals)
this move exists to get away from.

This is a pure relocation, not a rewrite: every class here keeps its
original API and design exactly as built and (for the I2C path) verified on
real hardware. See git history in both repos for the actual construction
story.

## OneMenu driven through these classes

A real IOP OneMenu instance, rendered via `Arduino_Wire_HAPI` bridged into
OneIO's own existing, already-OledOut-compatible `Ssd1306<Transport,W,H>`
driver (the *unrelated* native one in the parent `display/` directory) as
its byte-level I2C transport, lives at `OneIO/examples/arduinoGfxHapiMenu/`.
OneMenu's own output chain (`OledDisplay<>`, `GfxFmt<>`, font rendering) is
reused entirely unchanged, already proven on real AVR hardware by this
project's `u8g2Oled` example. `Arduino_Wire_HAPI` supplies the actual
hardware bus; nothing about OneMenu itself needed to change.

## Why this library, why this pair

Arduino_GFX solves a real combinatorial problem (~104 display controllers x
~74 data buses x ~12 MCU families) with an abstract `Arduino_DataBus` base
and pure-virtual dispatch on every command/pixel write -- including on 8-bit
AVR targets, where a vtable pointer and indirect calls are not free. ESP32 +
8-bit parallel + ST7789 was picked as the first target because: it's a real,
documented combination in the Arduino_GFX repo (see
`examples/PDQgraphicstest/Arduino_GFX_databus.h` there), the databus class
additionally caches a 256-entry `uint32_t` lookup table *per instance* at
runtime (`_xset_mask1`/`_xset_mask2`, up to 2KB RAM, flagged as expensive in
the original source's own comment), and the toolchain
(`xtensa-esp32-elf-g++`) is available locally to produce real numbers rather
than a synthetic estimate.

## Does HAPI's existing Chain/Traverse machinery fit this?

Checked before writing any of this. Short answer: no, and forcing it in
would be the wrong move here -- noting the gap rather than hacking around it.

HAPI's `Eval`/`Traverse`/`Apply`/`ApplyPack` (`HAPI/include/hapi/meta.h`)
solve a different problem: given a **heterogeneous list of mixed component
types** (a `hapi::Chain<A,B,C,...>`), recurse into it, apply a predicate or
transform to each element, and reassemble the result -- `SameAs`, `TagIs`,
`Filter`, `FindFirst`, `Map` and friends all exist to let OneMenu-style code
*query and filter* an open-ended pile of mixed components at compile time.
That machinery earns its keep when there's something to search: "does this
chain contain a component tagged X", "give me every component matching Q",
"transform every element of this chain".

The bus/display composition here is nothing like that. It's exactly two
fixed layers, each naming the next as an ordinary template parameter:
`Arduino_ST7789_HAPI<Bus>` holding a `Bus` member, `Bus` itself parameterized
on 12 pin NTTPs. There is no list to search, no predicate to apply, no
heterogeneous set of interchangeable parts to filter -- just one named type
wrapping another, which is what template parameterization (a.k.a.
policy-based design) already does directly. Routing this through
`hapi::Chain<>`/`Traverse` would mean modeling a 2-element list solely so a
query mechanism built for N-element introspection has something to operate
on -- machinery with no job to do, purely for the sake of using it.

If a future HAPI-composed display port grows a real "which of these N
optional post-processing stages are present" question (e.g. an optional
touch-controller layer, an optional DMA layer, a chain of color-format
converters where order/presence varies per board), *that* would be the
moment `Chain`/`Traverse`/`Filter` earns a place here. Until then, plain
template composition is the right-sized tool, and adding Chain-shaped
ceremony around a fixed 2-layer stack would be over-engineering (see IOP
memory: prefer three similar lines over a premature abstraction).

## Why not `constexpr` pointers for the PORT registers

Worth calling out because it's a genuine, verified-against-the-real-compiler
subtlety, not a hand-wave: `GPIO_OUT_W1TS_REG` and friends are fixed
addresses (e.g. `0x3FF44008`), known at compile time, but forming a pointer
from an integer address (`(PORTreg_t)GPIO_OUT_W1TS_REG`) is a
`reinterpret_cast`, and the C++ standard does not allow `reinterpret_cast`
in a core constant expression -- confirmed directly against
`xtensa-esp32-elf-g++ -std=gnu++17`:

```
error: reinterpret_cast from integer to pointer
  static constexpr PORTreg_t p = (PORTreg_t)GPIO_OUT_W1TS_REG;
```

So these addresses are **not** `static constexpr` data members here. They're
computed by small `static` `GFX_INLINE` (`always_inline`) functions
(`esp32par8_portSet`/`esp32par8_portClr`) called directly at each use site.
Verified this still folds to the same codegen a `constexpr` would have
produced (a single literal-pool load of the address, no stored pointer
variable, no indirection) -- see the disassembly in `Arduino_GFX/bench/
RESULTS.md`. The 256-entry lookup tables (`_xset_mask1`/`_xset_mask2`), by
contrast, only involve integer arithmetic (`digitalPinToBitMask` is `1UL <<
pin`, not a pointer cast), so those genuinely are `static constexpr
std::array<uint32_t,256>` -- .rodata, computed once at compile time, shared
by every instance of that exact pin combination, never RAM-resident.

## Scope of the display class

`Arduino_ST7789_HAPI<Bus>` sits on `Arduino_GFX_HAPI<Derived>` (see above),
so it gets the full `drawLine`/`drawCircle`/`drawArc`/`fillTriangle`/
`drawRoundRect`/classic-font `print`/`println` surface -- see
"PDQgraphicstest" below, which exercises all of it. It supplies: `begin`/
`tftInit` (replays the real init byte-sequence via `Arduino_HAPI_BatchOp.h`),
`setRotation` (rotations 0-3; the original's extended 4-7 diagonal-mirror
variants are rarely used and were left out), and the low-level hooks
(`writePixelPreclipped`, `writeFillRectPreclipped`, fast `writeFastHLine`/
`VLine`), each built from the identical `startWrite -> writeAddrWindow ->
bus-write -> endWrite` sequence as `Arduino_TFT`/the original.

`Arduino_ST7735_HAPI<Bus>` still only has the pixel-push hot path (`begin`/
`tftInit`/`setRotation`/`drawPixel`/`fillRect`/`fillScreen`) -- it predates
`Arduino_GFX_HAPI<Derived>` and hasn't been retrofitted onto it (not needed
for either deliverable so far; a mechanical follow-up, not a design gap).

`Arduino_SSD1306_HAPI<Bus,W,H>` mirrors the *original* `Arduino_SSD1306`'s own
scope instead (it never had a pixel/text API either): `begin`/`drawBitmap`/
`displayOn`/`displayOff`/`setBrightness`, where `drawBitmap` expects an
already-rendered 1bpp buffer in SSD1306 page/column order, same as upstream.
The OneMenu integration above needed direct column-by-column text writes
instead (a very different, no-local-framebuffer shape than "blit a whole
prepared buffer"), so it drives the I2C protocol itself via OneIO's own
native `Ssd1306<>` driver rather than through this class.

**Verified on real hardware** (2026-07-18): `Arduino_Wire_HAPI<0x3C,0x00,0x40>`
+ `Arduino_SSD1306_HAPI<Bus,128,64>`, flashed to a real LOLIN32 board's
onboard OLED (SDA=5, SCL=4 -- `Wire.begin(5,4)` before `oled.begin()`, same
as any sketch using non-default I2C pins). `oled.begin()` returned true (a
real I2C ACK from the physical SSD1306 at address 0x3C, not simulated), and
a 16px-wide alternating-bar test pattern sent via `drawBitmap` rendered
correctly on the physical display, confirmed visually (4 black + 4 white
bars, matching the pattern exactly). First HAPI class in this project
verified end-to-end on real hardware, not just compiled/linked.

## PDQgraphicstest: `USE_HAPI_BACKEND`

`Arduino_GFX/examples/PDQgraphicstest/PDQgraphicstest.ino` -- the library's
own one-example-every-board reference sketch -- has a `#ifdef
USE_HAPI_BACKEND` branch (undefined by default, so every existing board
target compiles and runs exactly as before): `#include
"Arduino_GFX_HAPI_backend.h"` (a new file, in that example's own folder,
which in turn includes the classes here) instead of the stock
`Arduino_GFX_pins.h`/`Arduino_GFX_databus.h`/`Arduino_GFX_display.h` path.
`gfx` ends up a real pointer to a concrete type
(`hapi_gfx::Arduino_ST7789_HAPI<HapiBus>*`), so every existing `gfx->` call
site in the rest of the file -- `drawLine`, `fillCircle`, `drawArc`,
`fillTriangle`, `print`/`println`, all ~21 methods the test scene uses --
keeps compiling completely unchanged.

**Bus choice, deliberately not normalized against the original.** The stock
path's default bus for generic ESP32 is `Arduino_ESP32SPI` (see
`Arduino_GFX_databus.h`) -- but that class unconditionally needs
`esp32-hal-periman.h`, which doesn't exist in *any* espressif32 platform
version available here (checked up to the newest, 7.0.1/Arduino-ESP32
3.20017): the stock SPI path does not compile in this environment at all,
independent of anything here. Rather than hand-port `Arduino_ESP32SPI`'s
own ~1200-line hardware-SPI-peripheral-plus-DMA driver (register layouts
differ per ESP32/S2/S3/C3/C6/P4 variant, unverifiable here without real
hardware) just to match the original's specific choice, the HAPI side uses
IOP's own existing, already-working SPI/pin primitives instead --
`hw::esp32::Esp32SpiMaster` (wraps the Arduino core's own `SPI` class) and
`Esp32OutPin` (wraps `pinMode`/`digitalWrite`), both from OneChip. See
`Arduino_IOPSPI_HAPI.h`'s own header comment. This means a real, honest
difference from the other buses here: `Esp32OutPin::set()` is a
`digitalWrite()` call (a runtime pin lookup), not a compile-time-folded
register address, and `Esp32SpiMaster::transfer()` goes through Arduino's
own `SPIClass`. Reported as-is below, not normalized to look more favorable.

**Numbers** (full scene: `fillScreen`, text, pixels, lines, rects, triangles,
circles, arcs, roundrects -- `esp32dev`, `-O2`, `-DUSE_HAPI_BACKEND`):

- RAM (`.dram0.bss`): 3920 B. Flash (`.flash.text`+`.flash.rodata`): 210571 B.
- **One 32-byte vtable** (`vtable for hapi_gfx::Arduino_ST7789_HAPI<...>`) --
  from inheriting Arduino's own `Print` class for `gfx->print()`/`println()`
  (`Print::write` is `virtual`). This is a cost the *original* `Arduino_GFX`
  pays too, for the identical reason (it also inherits `Print`) -- a shared,
  unavoidable cost from using the Arduino core's own text-output convenience,
  not a HAPI-vs-original asymmetry. No vtable exists for anything bus- or
  display-dispatch related (contrast with the six vtables -- `Arduino_G`,
  `Arduino_DataBus`, `Arduino_GFX`, `Arduino_TFT`, plus the concrete bus and
  display classes -- in the ESP32PAR8+ST7789 comparison in `Arduino_GFX/
  bench/RESULTS.md`).
- A handful of `callx8` instructions appear inside `writeFillArcHelper` and
  `write(uint8_t)`. Checked, not assumed: each loads its call target via a
  single `l32r` from a fixed literal-pool constant, not the vtable-dispatch
  pattern (`l32i` from the object to get a vptr, then a second `l32i` from
  the vptr to get a function pointer -- confirmed present in the *original*
  library's disassembly in `Arduino_GFX/bench/RESULTS.md`). These are
  Xtensa long calls (the callee is out of `call8`'s ~256KB PC-relative
  reach in this larger, primitive-laden binary), a binary-size/layout
  artifact, not polymorphism.
- No stock-path numbers for this exact board/bus/display combination are
  possible in this environment (see the periman.h note above) -- pending a
  newer Arduino-ESP32 core than any registry version currently offers.

Also found and fixed along the way, in `Arduino_ST7789_HAPI`'s fast
`writeFastHLine`/`writeFastVLine` overrides: the clipping in
`Arduino_TFT::writeFastVLine`/`HLine` is load-bearing, not decorative --
`writeEllipseHelper`/`fillTriangle`'s scanlines pass coordinates that run
outside the screen, and a first draft that skipped the clipping (a plain
1-wide/1-tall `writeFillRectPreclipped` passthrough) would have sent
out-of-range x/y straight to `CASET`/`RASET`. Ported the original's clipping
verbatim instead of the naive version.

## Reading the numbers

See `Arduino_GFX/bench/RESULTS.md` (in the Arduino_GFX checkout, not here)
for the actual `.text`/`.data`/`.bss` sizes and the side-by-side disassembly
of `fillScreen()`'s bus calls, original vs. HAPI, both built for
`esp32dev`/Arduino framework at the same optimization level -- the
ESP32PAR8+ST7789 pixel-write hot path comparison. See "PDQgraphicstest"
above for the full-primitives-surface comparison and its own bus choice.
