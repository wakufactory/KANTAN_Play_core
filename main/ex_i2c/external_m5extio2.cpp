#include "external_m5extio2.hpp"

#include <M5Unified.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------

bool external_m5extio2_t::init(uint8_t i2c_addr)
{
  _i2c_addr = i2c_addr;
  return scanID() && writeRegister(0x00, (const uint8_t[]){0,0,0,0, 0,0,0,0},8);
}

bool external_m5extio2_t::update(uint32_t &button_state)
{
  uint8_t readbuf[1];
  uint8_t hitcount = 0;
  uint32_t result = 0;
  for (int i = 0; i < 8; ++i) {
    if (M5.Ex_I2C.start(_i2c_addr, false, _i2c_freq)) {
      if (M5.Ex_I2C.write(0x20 + i)
      && M5.Ex_I2C.stop()
      && M5.Ex_I2C.start(_i2c_addr, true, _i2c_freq)
      && M5.Ex_I2C.read(readbuf, 1, true)) {
        ++hitcount;
      }
      M5.Ex_I2C.stop();
      result |= (!(bool)readbuf[0]) << i;
    }
  }
  if (hitcount != 8) {
    button_state = 0;
    return false;
  }
  button_state = result;
  return true;
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

