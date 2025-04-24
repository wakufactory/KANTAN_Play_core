#ifndef KANPLAY_TASK_MIDI_HPP
#define KANPLAY_TASK_MIDI_HPP

namespace kanplay_ns {
//-------------------------------------------------------------------------
class task_midi_t {
public:
  void start(void);
protected:
  static void task_func(task_midi_t* me);
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
