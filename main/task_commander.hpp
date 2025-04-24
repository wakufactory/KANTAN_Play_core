#ifndef KANPLAY_TASK_COMMANDER_HPP
#define KANPLAY_TASK_COMMANDER_HPP

#include "system_registry.hpp"

namespace kanplay_ns {
//-------------------------------------------------------------------------
class task_commander_t {
public:
  void start(void);
private:
  registry_t::history_code_t _internal_input_history_code = 0;
  registry_t::history_code_t _external_input_history_code = 0;
  static void task_func(task_commander_t* me);
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
