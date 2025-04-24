#include "interface_external.hpp"

#include <M5Unified.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------

bool interface_external_t::scanID(void)
{
  return M5.Ex_I2C.scanID(_i2c_addr);
}

bool interface_external_t::writeRegister8(uint8_t reg, uint8_t data)
{
  return M5.Ex_I2C.writeRegister8(_i2c_addr, reg, data, _i2c_freq);
}

bool interface_external_t::writeRegister(uint8_t reg, const uint8_t* data, size_t length)
{
  return M5.Ex_I2C.writeRegister(_i2c_addr, reg, data, length, _i2c_freq);
}

bool interface_external_t::readRegister(uint8_t reg, uint8_t* result, size_t length)
{
  return M5.Ex_I2C.readRegister(_i2c_addr, reg, result, length, _i2c_freq);
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

