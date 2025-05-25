// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#ifndef KANPLAY_EXTERNAL_M5EXTIO2_HPP
#define KANPLAY_EXTERNAL_M5EXTIO2_HPP

#include "interface_external.hpp"

namespace kanplay_ns {
//-------------------------------------------------------------------------
class external_m5extio2_t : public interface_external_t {
public:
  constexpr external_m5extio2_t(uint8_t addr = default_i2c_addr, uint32_t freq = 400000)
  : interface_external_t { addr, freq } {}

  static constexpr const uint8_t default_i2c_addr = 0x45;
  bool init(void) override;
  bool update(uint32_t &button_state) override;
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
