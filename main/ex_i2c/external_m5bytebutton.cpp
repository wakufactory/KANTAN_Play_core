#include "external_m5bytebutton.hpp"

#include <M5Unified.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------

bool external_m5bytebutton_t::init(void)
{
  uint8_t data = 0;
  bool result = (scanID() && readRegister(0x00, &data, 1));
  _exists = result;
  return result;
}

bool external_m5bytebutton_t::update(uint32_t &button_state)
{
  if (_exists) {
    uint8_t readbuf[1];
    if (readRegister(0x00, readbuf, 1)) {
      button_state |= (0xFF & ~readbuf[0]);
      return true;
    }
    _exists = false;
  }
  return false;
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

