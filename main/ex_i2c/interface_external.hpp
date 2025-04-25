#ifndef KANPLAY_INTERFACE_EXTERNAL_HPP
#define KANPLAY_INTERFACE_EXTERNAL_HPP

#include <stdint.h>
#include <stddef.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------
class interface_external_t {
public:
  constexpr interface_external_t(uint8_t addr = 0, uint32_t freq = 400000)
  : _i2c_freq { freq }, _i2c_addr { addr }, _exists { false } {}

  virtual bool init(void) = 0;
  virtual bool update(uint32_t &button_state) = 0;

  bool exists(void) const { return _exists; };

protected:
  uint32_t _i2c_freq = 0;
  uint8_t _i2c_addr = 0;
  bool _exists = false;
  bool scanID(void);
  bool writeRegister8(uint8_t reg, uint8_t data);
  bool writeRegister(uint8_t reg, const uint8_t* data, size_t length);
  bool readRegister(uint8_t reg, uint8_t* result, size_t length);
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
