#include "external_m5bytebutton.hpp"

#include <M5Unified.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------

bool external_m5bytebutton_t::init(uint8_t i2c_addr)
{
  _i2c_addr = i2c_addr;
  uint8_t data = 0;
  if (scanID() && readRegister(0x00, &data, 1)) {
    return true;
  }
  return false;
}

bool external_m5bytebutton_t::update(uint32_t &button_state)
{
  uint8_t readbuf[1];
  if (readRegister(0x00, readbuf, 1))
  {
    button_state = 0xFF & ~readbuf[0];
    return true;
  }
  button_state = 0;
  return false;
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

