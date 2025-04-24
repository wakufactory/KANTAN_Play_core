#include <M5Unified.h>

#include "task_port_a.hpp"

#include "common_define.hpp"
#include "system_registry.hpp"

#include "ex_i2c/external_m5bytebutton.hpp"
#include "ex_i2c/external_m5extio2.hpp"

namespace kanplay_ns {
//-------------------------------------------------------------------------

external_m5bytebutton_t external_m5bytebutton;
external_m5extio2_t external_m5extio2;

bool task_port_a_t::start(void)
{
  M5.Ex_I2C.begin();
#if defined (M5UNIFIED_PC_BUILD)
#else
  xTaskCreatePinnedToCore((TaskFunction_t)task_func, "port_a", 4096, this, def::system::task_priority_port_a, nullptr, def::system::task_cpu_port_a);
#endif
  return true;
}

void task_port_a_t::task_func(task_port_a_t* me)
{
  uint8_t loop_counter = 0;
  bool exist_bytebutton = false;
  bool exist_extio2 = false;
  uint8_t step = 0;

  for (;;) {
    M5.delay(2);
    if (++step == 2) {
      step = 0;
      ++loop_counter;
    }
    switch (step) {
    default: break;

    case 0:
      if (exist_extio2 == false) {
        if (loop_counter == 0) {
          exist_extio2 = external_m5extio2.init();
        }
      }
      else
      {
        uint32_t button_state = 0;
        exist_extio2 = external_m5extio2.update(button_state);
        system_registry.external_input.setExtIo2Bitmask(button_state);
      }
      break;
 
    case 1:
      if (exist_bytebutton == false) {
        if (loop_counter == 0) {
          exist_bytebutton = external_m5bytebutton.init();
        }
      }
      else
      {
        uint32_t button_state = 0;
        exist_bytebutton = external_m5bytebutton.update(button_state);
        system_registry.external_input.setByteButtonBitmask(button_state);
      }
      break;
    }
  }
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns
