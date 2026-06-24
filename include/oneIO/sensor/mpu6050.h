/**
 * @file mpu6050.h
 * @author Rui Azevedo (ruihfazevedo@gmail.com)
 * @brief MPU-6050 I2C IMU (accel + gyro) HAPI component.
 */

#pragma once
#include <hapi/hapi.h>

namespace oneSensor {

  /// @brief InvenSense MPU-6050 6-axis IMU (I2C); readAccel/readGyro return raw 16-bit values
  template<typename Bus, uint8_t Addr = 0x68>
  struct MPU6050 {
    struct Part {
      static void    init();
      static void    readAccel(int16_t& x, int16_t& y, int16_t& z);
      static void    readGyro (int16_t& x, int16_t& y, int16_t& z);
      static int16_t readTemp();
    };
  };

} // oneSensor
