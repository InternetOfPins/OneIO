# OneIO

IOP hardware I/O device drivers — displays, sensors, actuators. Pure-static HAPI components; no host-side buffers, no dynamic allocation.

## Display drivers

### SSD1306 OLED (128×64)

```cpp
#include <oneIO/display/i2cOled.h>   // I2C via Wire
#include <oneIO/display/spiOled.h>   // SPI

using MyOled = oneIO::display::I2cOledWire<Wire, SDA, SCL>;
MyOled::begin();
MyOled::clear();
MyOled::setCursor(0, 0);  // col in pixels, row in pages (8 px each)
MyOled::print('A');
MyOled::fillRect(0, 0, 128, 1, 0x00);
```

### HD44780 character LCD (I2C backpack, PCF8574)

```cpp
#include <oneIO/display/i2cLcd.h>

using MyLcd = oneIO::display::I2cLcd<AvrTwiMaster<>, 0x27>;
MyLcd::begin();
MyLcd::setCursor(0, 0);
MyLcd::print("Hello");
MyLcd::clear();
```

For direct parallel wiring, use `Hd44780<RS, EN, D4, D5, D6, D7>` with [OneBit](https://github.com/InternetOfPins/OneBit) `OutPin` types.

## Sensor drivers

### DS18B20 temperature (1-Wire)

```cpp
#include <oneIO/sensor/ds18b20.h>
```

### MPU6050 IMU (I2C)

```cpp
#include <oneIO/sensor/mpu6050.h>
```

## Integration with OneMenu

`OledDisplay<MyOled>` and `LcdDisplay<MyLcd, cols, rows>` from [OneMenu](https://github.com/InternetOfPins/OneMenu) wrap these drivers for menu output.

## Dependencies

- [HAPI](https://github.com/InternetOfPins/HAPI)
- [OneBit](https://github.com/InternetOfPins/OneBit)
- [OnePin](https://github.com/InternetOfPins/OnePin)
- [OneChip](https://github.com/InternetOfPins/OneChip)
- [OneBus](https://github.com/InternetOfPins/OneBus)
