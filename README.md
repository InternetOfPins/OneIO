# OneIO

IOP hardware I/O device drivers — displays, sensors, EEPROM, PWM, RTC, RF. Pure-static HAPI components; no host-side buffers, no dynamic allocation.

Part of the [InternetOfPins](https://github.com/InternetOfPins) project family.

## Displays

### SSD1306 OLED — I2C (128×64)

```cpp
#include <oneIO/display/i2cOled.h>

// Arduino Wire (ESP32: SDA=5, SCL=4; AVR: default pins)
using MyOled = oneIO::display::I2cOledWire<Wire, 5, 4>;
MyOled::begin();
MyOled::clear();
MyOled::setCursor(0, 0);  // col in pixels, page (0-7, 8 px each)
MyOled::print('H');
MyOled::fillRect(0, 0, 128, 1, 0x00);  // clear top page
```

For AVR with direct TWI: `I2cOled<AvrTwiMaster<100000UL, F_CPU>, 0x3C>`.

### SSD1306 OLED — SPI

```cpp
#include <oneIO/display/spiOled.h>
```

### ST7735 TFT — SPI (colour, OledOut-compatible)

```cpp
#include <oneIO/display/st7735.h>

// SpiSt7735<SpiMaster, CS, DC, RST, Width, Height, MADCTL, Xoffset, Yoffset>
using Tft = oneIO::display::SpiSt7735<MySpi, CS_PIN, DC_PIN, RST_PIN, 128, 160>;
Tft::begin();  // full init sequence (SWRESET, SLPOUT, FRMCTR, COLMOD, ...)
```

`ST7735` implements the same `OledOut` interface as `SSD1306`: `setCursor()`, `print()`, `fillRect()`, `setInverted()`. Use `oneMenu::OledDisplay<Tft>` as a drop-in for the OLED display in OneMenu.

### HD44780 character LCD — I2C (PCF8574 backpack)

```cpp
#include <oneIO/display/i2cLcd.h>

using MyLcd = oneIO::display::I2cLcd<TwiMaster, 0x27, 16, 2>;
MyLcd::begin();
MyLcd::setCursor(0, 0);
MyLcd::print('H');
MyLcd::clear();
```

### PCD8544 — Nokia 5110 LCD (SPI)

```cpp
#include <oneIO/display/pcd8544.h>
```

## Sensors

### AHT10 / AHT20 — I2C humidity + temperature

```cpp
#include <oneIO/sensor/aht.h>

using Sensor = oneIO::sensor::AHT<TwiMaster>;
Sensor::begin();
auto r = Sensor::sample();   // blocking: trigger + 80 ms wait + read
printf("%.1f °C  %.1f %%RH\n", r.tempC(), r.humidity());
```

### DS18B20 — 1-Wire temperature

```cpp
#include <oneIO/sensor/ds18b20.h>
#include <oneBus/oneWire.h>

using Bus  = oneBus::OneWire<4>;
using Temp = oneIO::sensor::DS18B20<Bus>;

Temp::begin();
Temp::trigger();       // start conversion (non-blocking)
// ... wait 750 ms ...
auto r = Temp::read(); // r.tempC(), r.ok
```

### MAX31855 — SPI thermocouple amplifier (K-type)

```cpp
#include <oneIO/sensor/max31855.h>

using TC  = oneIO::sensor::SpiMax31855<SpiMaster, CS_PIN>;
using Api = TC::Api;

Api::begin();
auto r = Api::read();
if (!r.fault()) printf("%.2f °C  cold=%.2f °C\n", r.tempC(), r.coldC());
```

Fault bits: `r.openCircuit()`, `r.shortGnd()`, `r.shortVcc()`.

### MPU6050 — I2C 6-axis IMU

```cpp
#include <oneIO/sensor/mpu6050.h>

using Imu = oneIO::sensor::MPU6050<TwiMaster>;
Imu::begin();
auto r = Imu::read();   // r.ax, r.ay, r.az, r.gx, r.gy, r.gz, r.tempC()
```

## EEPROM

### AT24Cxx — I2C EEPROM

```cpp
#include <oneIO/eeprom/at24c.h>

// AT24C32: 4 kB, 32-byte pages, address 0x50
using Eep = oneIO::eeprom::AT24CWire<Wire, SDA, SCL, 0x50, 32>;
Eep::begin();
Eep::write(0, buf, len);
Eep::read(0, buf, len);
```

### EepromBlock — wear-levelled structured store

Wraps any EEPROM backend with CRC32 validation, version signatures, and block-recycle wear levelling.

```cpp
#include <oneIO/eeprom/store.h>

struct Settings { int bright; int speed; };

// 256 bytes assigned, 32-byte pages → 8 recycle cells
using Store = oneIO::eeprom::EepromBlock<Eep, Settings, 256>;

Store::begin();                     // scan: find newest valid cell
if (Store::valid()) {
  Settings s; Store::load(s);       // restore from EEPROM
}
Store::save(currentSettings);       // advance cell + write + CRC
```

Pair with `AvrEeprom<1024>` (ATmega328P internal) or `AT24CWire<>` for external flash.

## PWM

### PCA9685 — I2C 16-channel 12-bit PWM driver

```cpp
#include <oneIO/pwm/pca9685.h>

using Pwm = oneIO::pwm::PCA9685<TwiMaster, 0x40>;
Pwm::begin(50);             // 50 Hz (servo)
Pwm::setChannel(0, 307);    // channel 0, ~1.5 ms pulse (centre)
```

## RTC

### DS3231 — I2C real-time clock

```cpp
#include <oneIO/rtc/ds3231.h>
```

## RF

### TinyRC — PPM RC transmitter/receiver

```cpp
#include <oneIO/rf/tinyRC.h>
```

### OneSw — simple single-wire RF protocol

```cpp
#include <oneIO/rf/oneSw.h>
```

## Storage

### SD card (SPI)

```cpp
#include <oneIO/storage/sdcard.h>
```

## Integration with OneMenu

`OledDisplay<MyOled>`, `OledDisplay<Tft>`, and `LcdDisplay<MyLcd>` from [OneMenu](https://github.com/InternetOfPins/OneMenu) wrap display drivers for menu output.

## Dependencies

- [HAPI](https://github.com/InternetOfPins/HAPI)
- [OneBit](https://github.com/InternetOfPins/OneBit)
- [OneChip](https://github.com/InternetOfPins/OneChip)
- [OneBus](https://github.com/InternetOfPins/OneBus)

## License

MIT — see [LICENSE](LICENSE).

*Author: Rui Azevedo (neu-rah) · Azores, Portugal*
