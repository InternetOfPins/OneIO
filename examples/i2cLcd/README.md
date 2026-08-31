# i2cLcd — HD44780 16x2 over a PCF8574 I2C backpack

`oneIO::display::I2cLcd<Twi, Addr>` composed over a platform `Twi` master.
One source for AVR and STM32; prints a banner + a 1 Hz uptime counter.
Hardware-verified on an Uno (ATmega328) and a Blue Pill (STM32F103C8).

## Wiring

| Signal | AVR (Uno/Nano) | STM32 (Blue Pill) |
|--------|----------------|-------------------|
| SCL    | A5             | PB6               |
| SDA    | A4             | PB7               |
| VCC    | 5V             | 5V                |
| GND    | GND            | GND               |

PB6/PB7 are 5V-tolerant open-drain; the backpack carries its own pull-ups.
Address `0x27` (PCF8574) — pass `-DLCD_ADDR=0x3F` for a PCF8574A backpack.

A standard 1602 panel needs ~5 V to develop contrast. On a Blue Pill powered
from an ST-Link's 3.3 V the `5V` pin sits at ~3.2 V — the PCF8574 still ACKs
but the glass stays blank at any contrast setting. Power the board over USB.

## Build / flash

```sh
pio run -e avr -t upload                 # AVR

pio run -e bluepill_72                   # STM32 @ 72 MHz (HSE x9)
pio run -e bluepill_64                   # STM32 @ 64 MHz (HSI/2 x16)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program .pio/build/bluepill_72/firmware.elf verify reset exit"
```

## The two STM32 clocks

`stm32f1_pll_72mhz()` (in `OneChip/chips/stm32/stm32SysClock.h`) uses the
on-board HSE crystal with a timeout fall-back to HSI. `stm32f1_pll_64mhz()`
uses HSI/2 x16 — no crystal dependency — which is also exactly what
`stm32f1xx-hal`'s `sysclk(72.MHz())` delivers on HSI (it clamps to 64). The
Rust bridge (`HAPI/examples/rust_stm32_bridge`) reuses the 64 MHz config, so
this example covers that clock/I2C combination in pure C++ as well.

APB1 (the I2C1 clock) is HCLK/2 either way — 36 MHz @ 72, 32 MHz @ 64 —
passed as `chip::Twi<100000, APB1_HZ>` so `Stm32I2cCore` derives CCR/TRISE
from the real clock. `hd44780.h`'s delay loop is calibrated near 72 MHz; at
64 MHz it over-delays, which the HD44780 tolerates (only under-delay breaks
init).

## Verified register state (real STM32F103C8, OpenOCD)

Every LCD/I2C call in `main` compiles to a direct `bl` to a monomorphized
symbol — no vtable, no `blx` register dispatch, no typeinfo in the image.

| Register       | @ 72 MHz (HSE x9)                         | @ 64 MHz (HSI/2 x16)                        |
|----------------|------------------------------------------|--------------------------------------------|
| `FLASH_ACR`    | LATENCY=2                                 | LATENCY=2                                   |
| `RCC_CFGR`     | SWS=PLL, SRC=HSE, MUL=x9, PPRE1=/2        | SWS=PLL, SRC=HSI/2, MUL=x16, PPRE1=/2       |
| `SysTick_LOAD` | 71999                                     | 63999                                       |
| `I2C1_CR2` FREQ| 36                                        | 32                                          |
| `I2C1_CCR`     | 180                                       | 160                                         |
| `I2C1_TRISE`   | 37                                        | 33                                          |

## F1 I2C note

The STM32F1 I2C block locks up with `BUSY` stuck after sustained
back-to-back transfers (the LCD does ~5 single-byte writes per nibble).
`OneChip/chips/stm32/stm32Twi.h` handles it: `end_write()` waits for `BUSY`
to clear after `STOP` so the next transfer starts on an idle bus, every SR
poll is bounded, and a stall / `BERR` / `ARLO` / `AF` triggers `recover()`
(SWRST + re-init) and bumps a public `fault_count`. Soak: 72 MHz for ~130 s,
64 MHz for ~90 s, `fault_count == 0` (the bus-idle wait alone prevents the
lockup; `recover()` is a backstop).
