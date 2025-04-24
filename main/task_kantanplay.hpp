#ifndef KANPLAY_TASK_KANTANPLAY_HPP
#define KANPLAY_TASK_KANTANPLAY_HPP

#include "system_registry.hpp"

namespace kanplay_ns {
//-------------------------------------------------------------------------
class task_kantanplay_t {
public:
  void start(void);
private:
  registry_t::history_code_t _player_command_history_code = 0;
  static void task_func(task_kantanplay_t* me);
  bool commandProccessor(void);

  void chordStep(bool on_beat);
  void chordPlay(bool on_beat);
  void chordStepSet(int step);
  void chordNoteOff(int part);
  uint32_t chordProc(void);
  void procSoundEffect(const def::command::command_param_t& command_param, const bool is_pressed);
  void procChordDegree(const def::command::command_param_t& command_param, const bool is_pressed);
  void procNoteButton(const def::command::command_param_t& command_param, const bool is_pressed);
  void procDrumButton(const def::command::command_param_t& command_param, const bool is_pressed);
  void procChordStepResetRequest(const def::command::command_param_t& command_param, const bool is_pressed);

  void setPitchManage(uint8_t part, uint8_t pitch, uint8_t midi_ch, uint8_t note_number, int8_t velocity, int32_t press_usec, int32_t release_usec);

  void updateTempoSwing(void);

  struct midi_pitch_manage_t
  {
    int32_t press_usec;
    int32_t release_usec;
    uint8_t midi_ch;
    uint8_t note_number;
    uint8_t velocity;
  };
  // ピッチごとの演奏情報 (履歴を最大2個分持てるようにする)
  // 履歴の配列は 0 が古い。max_manage_history - 1 が最新
  static constexpr const size_t max_manage_history = 3;
  midi_pitch_manage_t _midi_pitch_manage[def::app::max_chord_part][def::app::max_pitch_with_drum][max_manage_history];
  int32_t checkOtherPitchNote(int part, int pitch, int midi_ch, int note_number);

  struct midi_note_manage_t
  {
    uint8_t midi_ch = 0;
    uint8_t note_number = 0;
  };
  midi_note_manage_t _midi_note_manage[def::hw::max_main_button];

  registry_t::history_code_t _slot_index;
  uint32_t _prev_usec = 0;
  uint32_t _current_usec = 0;

  def::play::auto_play_mode_t _auto_play;

  // 自動でアルペジエータが先頭に戻るまでのタイムアウト残り時間(マイクロ秒)
  int32_t _arpeggio_reset_remain_usec = -1;

  // ユーザ手入力タイミングによる自動演奏サイクルの判定用
  size_t _auto_play_recording_index = 0;
  int _auto_play_recording_usec[8] = {0};

  int _auto_play_remain_usec = -1;
  int _auto_play_onbeat_usec = 250 * 1000;
  int _auto_play_offbeat_usec = 250 * 1000;
  bool _is_on_beat = false;

  // 演奏時のベロシティ
  uint8_t _press_velocity;

  // 現在のDegree(度数)
  uint8_t _degree;
  uint8_t _bass_degree;
  int8_t _bass_semitone;
  bool _step_reset_request = false;
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
