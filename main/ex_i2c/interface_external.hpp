#ifndef KANPLAY_INTERFACE_EXTERNAL_HPP
#define KANPLAY_INTERFACE_EXTERNAL_HPP

#include <stdint.h>
#include <stddef.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------
class interface_external_t {
public:
protected:
  uint32_t _i2c_freq = 400000;
  uint8_t _i2c_addr;
  bool scanID(void);
  bool writeRegister8(uint8_t reg, uint8_t data);
  bool writeRegister(uint8_t reg, const uint8_t* data, size_t length);
  bool readRegister(uint8_t reg, uint8_t* result, size_t length);
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
