#ifndef KANPLAY_EXTERNAL_M5EXTIO2_HPP
#define KANPLAY_EXTERNAL_M5EXTIO2_HPP

#include "interface_external.hpp"

namespace kanplay_ns {
//-------------------------------------------------------------------------
class external_m5extio2_t : public interface_external_t {
public:
  static constexpr const uint8_t default_i2c_addr = 0x45;
  bool init(uint8_t i2c_addr = default_i2c_addr);
  bool update(uint32_t &button_state);
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
