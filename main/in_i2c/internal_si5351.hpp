#ifndef KANPLAY_INTERNAL_SI5351_HPP
#define KANPLAY_INTERNAL_SI5351_HPP

#include <inttypes.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------
class internal_si5351_t {
public:
  void init(uint8_t cap = 10, uint32_t xtal = 27000000);
  void update(int freq);
private:
  uint32_t _xtal_freq = 27000000;
  uint8_t _clkin_div = 0;
  uint8_t _ref_correction = 0;
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
