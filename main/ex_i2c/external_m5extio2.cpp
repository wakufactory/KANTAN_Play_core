// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#include "external_m5extio2.hpp"

#include <M5Unified.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------

bool external_m5extio2_t::init(void)
{
  bool result = (scanID() && writeRegister(0x00, (const uint8_t[]){0,0,0,0, 0,0,0,0},8));
  _exists = result;
  return result;
}

bool external_m5extio2_t::update(uint32_t &button_state)
{
  if (_exists) {
    uint8_t readbuf[1];
    uint_fast8_t hitcount = 0;
    uint_fast8_t result = 0;
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
    if (hitcount == 8) {
      button_state |= result;
      return true;
    }
    _exists = false;
  }
  return false;
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

