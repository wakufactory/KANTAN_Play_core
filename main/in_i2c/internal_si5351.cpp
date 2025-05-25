// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#include "internal_si5351.hpp"

#include <M5Unified.h>

namespace kanplay_ns {
//-------------------------------------------------------------------------

static constexpr const uint32_t i2c_freq = 400000;
static constexpr const uint8_t i2c_addr = 0x60;

static bool writeRegister8(uint8_t reg, uint8_t data)
{
  return M5.In_I2C.writeRegister8(i2c_addr, reg, data, i2c_freq);
}

static bool writeRegister(uint8_t reg, const uint8_t* data, size_t length)
{
  return M5.In_I2C.writeRegister(i2c_addr, reg, data, length, i2c_freq);
}

static bool readRegister(uint8_t reg, uint8_t* result, size_t length)
{
  return M5.In_I2C.readRegister(i2c_addr, reg, result, length, i2c_freq);
}

static uint8_t readRegister8(uint8_t reg)
{
  return M5.In_I2C.readRegister8(i2c_addr, reg, i2c_freq);
}

#if 0
static void disable_output(void)
{
  writeRegister8( 3, 0xff);  // OUTPUT_ENABLE_CONTROL
  writeRegister8(16, 0x80);  // CLK0_CONTROL
  writeRegister8(17, 0x80);  // CLK1_CONTROL
  writeRegister8(18, 0x80);  // CLK2_CONTROL
}

static void enable_output(void)
{
  writeRegister8( 3, 0x00);  // OUTPUT_ENABLE_CONTROL
}

static void reset_pll(void)
{
  writeRegister8(177, 0xAC);  // PLL_RESET
}

static void setup_pll(uint8_t pll, /* PLL_A or PLL_B */
                     uint8_t  mult,
                     uint32_t num,
                     uint32_t denom)
{
  static constexpr const uint8_t pll_reg_base[] = {
    26, // PLL_A,
    34, // PLL_B
  };
  uint8_t baseaddr = pll_reg_base[pll & 1];

  uint32_t P1;
  uint32_t P2;
  uint32_t P3;

  /* Set the main PLL config registers */
  if (num == 0) {
    /* Integer mode */
    P1 = 128 * mult - 512;
    P2 = 0;
    P3 = 1;
  } else {
    /* Fractional mode */
    P1 = 128 * mult + ((128 * num) / denom) - 512;
    P2 = 128 * num - denom * ((128 * num) / denom);
    P3 = denom;
  }

  /* The datasheet is a nightmare of typos and inconsistencies here! */
  writeRegister8(baseaddr,   (P3 & 0x0000FF00) >> 8);
  writeRegister8(baseaddr+1, (P3 & 0x000000FF));
  writeRegister8(baseaddr+2, (P1 & 0x00030000) >> 16);
  writeRegister8(baseaddr+3, (P1 & 0x0000FF00) >> 8);
  writeRegister8(baseaddr+4, (P1 & 0x000000FF));
  writeRegister8(baseaddr+5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16) );
  writeRegister8(baseaddr+6, (P2 & 0x0000FF00) >> 8);
  writeRegister8(baseaddr+7, (P2 & 0x000000FF));
}
#endif


static void bulk_write(const uint8_t* reg_data)
{
  while (*reg_data) {
    uint8_t len = *reg_data++;
    int retry = 16;
    while (!writeRegister(reg_data[0], &reg_data[1], len - 1) && --retry) { M5.delay(1); }
    reg_data += len;
  }
}

void internal_si5351_t::init(uint8_t cap, uint32_t xtal)
{
  int retry = 1024;
  while ((writeRegister8(3, 0x80) & 0x80) && --retry) { M5.delay(1); }
  if (retry == 0) {
    M5_LOGE("Si5351 busy");
    return;
  }

  const uint8_t init_config[] = {
    2,   3, 0xff,              // OUTPUT_ENABLE_CONTROL : disable all outputs
    4,  16, 0x80, 0x80, 0x80,  // CLK0_CONTROL : disable,disable,disable
    2, 183, 0xC0 ,             // CRYSTAL_LOAD : 10PF
    9,  26, 0xFF, 0xFD, 0x00, 0x09, 0x26, 0xF7, 0x4F, 0x72,  // PLL_A setup
    2, 177, 0xA0,  // PLL_RESET : reset A and B

    // setup multisynth 6.144MHz
    9, 50,  0x00, 0x01, 0x00, 0x2F, 0x00, 0x00, 0x00, 0x00, // MULTISYNTH1

    // CLK1_CONTROL   (DRIVE_STRENGTH_2MA == 0<<0) | (CLK_INPUT_MULTISYNTH_N == 3<<2) | (INTEGER_MODE	== 1<<6)
    2, 17,  (3<<2) | (1<<6),
    2,  3, 0x00,              // OUTPUT_ENABLE_CONTROL : enable all outputs
    0 // sentinel
  };
  bulk_write(init_config);
}

void internal_si5351_t::update(int freq) {
  // writeRegister8(0x03, 0x00);  // OUTPUT_ENABLE_CONTROL : enable all outputs
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

