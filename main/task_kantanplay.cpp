#include <M5Unified.h>

#include "common_define.hpp"

#include "task_kantanplay.hpp"
#include "system_registry.hpp"

#include "kantan-music/include/KANTANMusic.h"

namespace kanplay_ns {
//-------------------------------------------------------------------------

void task_kantanplay_t::start(void)
{
  memset(_midi_pitch_manage, 0xFF, sizeof(_midi_pitch_manage));

  // MIDIチャンネルの初期値設定 (暫定本来はスロット設定から取得する)
  // for (int i = 0; i < 16; ++i) {
  //   system_registry.midi_out_control.set8(system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_VOLUME_CH1 + i, 100);
  //   // system_registry.midi_out_control.set8(system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_PROGRAM_CH1 + i, 0);
  // }

  _current_usec = M5.micros();

#if defined (M5UNIFIED_PC_BUILD)
  auto thread = SDL_CreateThread((SDL_ThreadFunction)task_func, "kanplay", this);
#else
  TaskHandle_t handle = nullptr;
  xTaskCreatePinnedToCore((TaskFunction_t)task_func, "kanplay", 1024*3, this, def::system::task_priority_kantanplay, &handle, def::system::task_cpu_kantanplay);
  system_registry.player_command.setNotifyTaskHandle(handle);
#endif
}

void task_kantanplay_t::task_func(task_kantanplay_t* me)
{
  for (;;) {
    me->_prev_usec = me->_current_usec;
    me->_current_usec = M5.micros();

    int32_t next_usec = me->chordProc();
    if (me->commandProccessor()) {
      me->_prev_usec = me->_current_usec;
      next_usec = me->chordProc();
    }
#if !defined (M5UNIFIED_PC_BUILD)
    taskYIELD();
#endif
    int next_msec = (next_usec + 128) >> 10;
    {
      system_registry.task_status.setSuspend(system_registry_t::reg_task_status_t::bitindex_t::TASK_KANTANPLAY);
#if defined (M5UNIFIED_PC_BUILD)
      M5.delay(1);
#else
      ulTaskNotifyTake(pdTRUE, next_msec);
#endif
      system_registry.task_status.setWorking(system_registry_t::reg_task_status_t::bitindex_t::TASK_KANTANPLAY);
    }
  }
}

bool task_kantanplay_t::commandProccessor(void)
{
  bool res = false;
  def::command::command_param_t command_param;
  bool is_pressed;
  while (system_registry.player_command.getQueue(&_player_command_history_code, &command_param, &is_pressed))
  {
    res = true;
    switch (command_param.getCommand()) {
    default:
      break;
    case def::command::set_velocity:
      if (is_pressed)
      {
        const auto param = command_param.getParam();
        system_registry.runtime_info.setPressVelocity(param);
        _press_velocity = param;
      }
      break;
    case def::command::sound_effect:
      procSoundEffect(command_param, is_pressed);
      break;
    case def::command::chord_degree:
      procChordDegree(command_param, is_pressed);
      break;
    case def::command::note_button:
      procNoteButton(command_param, is_pressed);
      break;
    case def::command::drum_button:
      procDrumButton(command_param, is_pressed);
      break;
    case def::command::chord_step_reset_request:
      procChordStepResetRequest(command_param, is_pressed);
      break;

    case def::command::autoplay_toggle:
      if (is_pressed)
      { // 自動演奏モードのオン・オフのトグル
        auto autoplay = system_registry.runtime_info.getChordAutoplayState();
        if (autoplay == def::play::auto_play_mode_t::auto_play_none) {
          autoplay = def::play::auto_play_mode_t::auto_play_waiting;
  // M5_LOGV("looping_msec: %d, onbeat_msec: %d  bpm:%d", looping_usec / 1000, onbeat_usec / 1000, 60000000 / looping_usec);
        } else {
          autoplay = def::play::auto_play_mode_t::auto_play_none;
        }
     // M5_LOGV("autoplay %d", (int)autoplay);
        system_registry.runtime_info.setChordAutoplayState(autoplay);
      }
      break;
    }
  }
  return res;
}

void task_kantanplay_t::updateTempoSwing(void)
{
  auto tempo = system_registry.song_data.song_info.getTempo();
  if (tempo < def::app::tempo_bpm_min) { tempo = def::app::tempo_bpm_default; }

  uint8_t swing = system_registry.song_data.song_info.getSwing();
  if (swing > def::app::swing_percent_max) { swing = def::app::swing_percent_max; }

  // swing 0のとき on_beat:off_beat = 1:1、swing 100のとき on_beat:off_beat = 2:1 となるようにする
  swing = 50 + (swing / 6); // 0-100 -> 50-66

  uint32_t cycle_usec = (60 * 1000 * 1000 + (tempo >> 1)) / tempo;
  uint32_t onbeat_usec = cycle_usec * swing / 100;
  uint32_t offbeat_usec = cycle_usec - onbeat_usec;

// M5_LOGV("tempo:%d, swing:%d, cycle:%d, onbeat:%d, offbeat:%d", tempo, swing, cycle_usec, onbeat_usec, offbeat_usec);

  _auto_play_onbeat_usec = onbeat_usec;
  _auto_play_offbeat_usec = offbeat_usec;
}

void task_kantanplay_t::procSoundEffect(const def::command::command_param_t& command_param, const bool is_pressed)
{
  // const auto command = command_param.getCommand();
  auto effect_type = (def::command::sound_effect_t)command_param.getParam();
  int master_key = system_registry.runtime_info.getMasterKey();
  int slot_key = master_key + (int8_t)system_registry.current_slot->slot_info.getKeyOffset();
  while (slot_key < 0) { slot_key += 12; }
  while (slot_key >= 12) { slot_key -= 12; }
  int degree = _degree;
  if (degree < 1) { degree = 1; }
  else if (degree > 7) { degree = 7; }

  auto part_index = system_registry.chord_play.getEditTargetPart();
  auto chord_part = &system_registry.current_slot->chord_part[part_index];
  auto part_info = &chord_part->part_info;
  int pitch_index = system_registry.chord_play.getCursorY();
  int step = system_registry.chord_play.getPartStep(part_index);

  int pitch_last = pitch_index + 1;
  uint8_t midi_ch = part_index;

  if (!part_info->isDrumPart() && pitch_index >= def::app::max_pitch_without_drum) {
    effect_type = def::command::sound_effect_t::testplay;
  }

  KANTANMusic_GetMidiNoteNumberOptions options;
  KANTANMusic_GetMidiNoteNumber_SetDefaultOptions(&options);
  options.minor_swap = system_registry.chord_play.getChordMinorSwap();
  options.semitone_shift = system_registry.chord_play.getChordSemitone();
  options.modifier = system_registry.chord_play.getChordModifier();
  options.voicing = part_info->getVoicing();
  options.position = part_info->getPosition();

  // プレビュー再生時の音は 500msecとする
  int autorelease_usec = 1000 * 500;

  int displacement_usec = 1000 * part_info->getStrokeSpeed();
  int pitch_flow = 1;
  bool mute = false;

  switch (effect_type) {
  default:
    break;

  case def::command::sound_effect_t::testplay:
    {
      int step = system_registry.chord_play.getPartStep(part_index);
      switch (chord_part->arpeggio.getStyle(step))
      {
      default: break;
      case def::play::arpeggio_style_t::same_time:
        pitch_flow = 1;
        pitch_index = 0;
        pitch_last = def::app::max_pitch_with_drum;
        displacement_usec = 0;
        break;

      case def::play::arpeggio_style_t::high_to_low:
        pitch_flow = 1;
        pitch_index = 0;
        pitch_last = def::app::max_pitch_with_drum;
        break;

      case def::play::arpeggio_style_t::low_to_high:
        pitch_flow = -1;
        pitch_index = def::app::max_pitch_with_drum - 1;
        pitch_last = -1;
        break;

      case def::play::arpeggio_style_t::mute:
        mute = true;
        // ミュート処理は同時発音ではなく高速ダウンストロークとして扱う

        // ミュート処理の時はリリースまでの時間はストロークスピードの 2倍とする
        autorelease_usec = displacement_usec * 2;

        // ミュート処理の時はストロークスピードは 1/4 とする。
        displacement_usec >>= 2;

        pitch_flow = -1;
        pitch_index = def::app::max_pitch_with_drum - 1;
        pitch_last = -1;
        break;
      }
    }
    break;
  }

  uint32_t note = 0;
  uint32_t press_usec = 0;
  for (; pitch_index != pitch_last; pitch_index += pitch_flow) {
    int velocity = chord_part->arpeggio.getVelocity(step, pitch_index);
    if (effect_type == def::command::sound_effect_t::testplay) {
      if (velocity == 0 && !mute) {
        continue;
      }
    } else {
      if (velocity == 0)
      { // OFFドットのプレビュー音は ベロシティ 64 を 100msecとする
        velocity = 64;
        autorelease_usec = 1000 * 100;
      } else if (velocity < 0)
      { // ミュートドットのプレビュー音は ベロシティ 32 を 50msecとする
        velocity = 32;
        autorelease_usec = 1000 * 50;
      }
    }

    if (part_info->isDrumPart()) {
      note = system_registry.song_data.chord_part_drum[part_index].getDrumNoteNumber(pitch_index);
      midi_ch = def::midi::channel_10;
    } else {
      if (pitch_index < 0 || pitch_index > 5) { continue; }

      note = KANTANMusic_GetMidiNoteNumber(
          6 - pitch_index
        , degree
        , slot_key
        , &options
      );
    }
    setPitchManage(part_index, pitch_index, midi_ch, note, velocity, press_usec, press_usec + autorelease_usec);
    press_usec += displacement_usec;
    uint8_t program = part_info->getTone();
    uint8_t chvolume = part_info->getVolume() * 127 / 100;
    if (chvolume > 127) { chvolume = 127; }
    system_registry.midi_out_control.setProgramChange(midi_ch, program);
    system_registry.midi_out_control.setChannelVolume(midi_ch, chvolume);
  }
}

void task_kantanplay_t::procChordDegree(const def::command::command_param_t& command_param, const bool is_pressed)
{
  const bool on_beat = is_pressed;
  const uint8_t degree = command_param.getParam();

  int current_degree = system_registry.chord_play.getChordDegree();

  // 現在のDegreeと異なる場合
  if (current_degree != degree) {
    // ボタンを離した場合は何もしない
    if (!on_beat) {
      return;
    }
    system_registry.chord_play.setChordDegree(degree);
  }
  auto auto_play = system_registry.runtime_info.getChordAutoplayState();

  switch (auto_play) {
  default: break;
  case def::play::auto_play_mode_t::auto_play_none:
  case def::play::auto_play_mode_t::auto_play_running:
    _auto_play_recording_index = 0;
    break;

  case def::play::auto_play_mode_t::auto_play_waiting:
    _auto_play_remain_usec = 0;
    auto_play = def::play::auto_play_mode_t::auto_play_running;
    // updateTempoSwing();
    system_registry.runtime_info.setChordAutoplayState(auto_play);
    break;

/* // 連打間隔に基づいて自動演奏のサイクルを決定する
  case def::play::auto_play_mode_t::auto_play_recording:
    if ((_auto_play_recording_index & 1) == on_beat) {
      break;
    }

    _auto_play_recording_usec[_auto_play_recording_index] = _current_usec;
    if (++_auto_play_recording_index >= 8) {
      _auto_play_recording_index = 0;
      uint32_t looping_usec = 0;
      uint32_t onbeat_usec = 0;
      for (int i = 0; i < 6; i += 2) {
        // 押し下げ時間の間隔
        looping_usec += _auto_play_recording_usec[i + 2] - _auto_play_recording_usec[i];

        // 押し下げから離しまでの間隔
        onbeat_usec += _auto_play_recording_usec[i + 1] - _auto_play_recording_usec[i];
      }
      looping_usec /= 3;
      onbeat_usec /= 3;
      _auto_play_onbeat_usec = onbeat_usec;
      _auto_play_offbeat_usec = looping_usec - onbeat_usec;
M5_LOGV("looping_msec: %d, onbeat_msec: %d  bpm:%d", looping_usec / 1000, onbeat_usec / 1000, 60 * 1000 * 1000 / looping_usec);
      _auto_play_remain_usec = 0;
      auto_play = def::play::auto_play_mode_t::auto_play_running;
      system_registry.runtime_info.setChordAutoplayState(auto_play);
    }
    break;
//*/
  }
  if (auto_play != def::play::auto_play_mode_t::auto_play_running) {
    _is_on_beat = on_beat;
    chordStep(on_beat);
    chordPlay(on_beat);
  }
}

void task_kantanplay_t::procChordStepResetRequest(const def::command::command_param_t& command_param, const bool is_pressed)
{
  if (_is_on_beat) {
    _step_reset_request = true;
  } else {
    _step_reset_request = false;
    for (int i = 0; i < def::app::max_chord_part; ++i) {
      int banlift = system_registry.current_slot->chord_part[i].part_info.getAnchorStep();
      if (banlift < system_registry.chord_play.getPartStep(i))
      {
        system_registry.chord_play.setPartStep(i, 0);
      }
    }
  }
}

void task_kantanplay_t::chordStepSet(int step)
{
  // if (step <= 0) { _step_reset_request = false; }
  // for (int i = 0; i < def::app::max_chord_part; ++i) {
  //   system_registry.chord_play.setPartStep(i, step);
  // }
}

void task_kantanplay_t::chordStep(bool on_beat)
{
  // パートの有効無効の切り替えタイミングになったか否かをチェックする
  // アルペジオパターンの先頭に戻ったタイミングでパネルの有効無効をチェンジする

/*
●パターンの演奏ステップが先頭に戻るタイミング
 - 強制的に先頭に戻るタイミング
   - パターン終端に到達した場合
   - スロットが変更になった場合
   - 無操作で一定時間経過した場合 (自動演奏時と編集時を除く)

 - on_beat時に先頭に戻る可能性があるタイミング
   - Degreeが変更になった場合
   - マイナースワップやセミトーンシフトが変更になった場合
   ↑ (ステップ位置がループ禁止範囲にある場合は許可しない)
     (ステップ先頭裏拍にある場合は許可しない)

●パートがオンからオフに変更になるタイミング
 - ループ先頭に戻った時

●パートがオフからオンに変更になるタイミング
 - on_beat時
   - 現在有効なパートがひとつもない場合
   - 半数以上のパートが先頭にある場合
*/

  bool reset_request = false;
  bool force_reset = false;
  auto slot_index = system_registry.runtime_info.getPlaySlot();
  // auto history_code = system_registry.current_slot->slot_info.getHistoryCode();
  if (on_beat)
  { // オモテ拍でのみ、先頭に戻すリクエストを受け付ける
    reset_request = _step_reset_request;
    force_reset = reset_request;
    _step_reset_request = false;

    // Degreeが変更になっている場合は先頭に戻すリクエストフラグを立てる(強制ではない)
    int degree = system_registry.chord_play.getChordDegree();
    if (_degree != degree) {
      // Degreeが変更になった場合は前回のDegreeのボタンを消灯する
      system_registry.working_command.clear( { def::command::chord_degree, _degree } );
      _degree = degree;
      reset_request = true;
    }

    // マイナースワップや半音上げ下げが変更になった場合は先頭に戻すリクエストフラグを立てる(強制ではない)
    int minor_swap = system_registry.chord_play.getChordMinorSwap();
    int semitone = system_registry.chord_play.getChordSemitone();
    if (_minor_swap != minor_swap || _semitone != semitone) {
      _minor_swap = minor_swap;
      _semitone = semitone;
      reset_request = true;
    }

    // オンコード用のベースDegreeが変更になった時は先頭に戻すリクエストフラグを立てる(強制ではない)
    int base_degree = system_registry.chord_play.getChordBaseDegree();
    if (_bass_degree != base_degree) {
      _bass_degree = base_degree;
      reset_request = true;
    }

    // オンコード用のベースSemitoneが変更になった時は先頭に戻すリクエストフラグを立てる(強制ではない)
    int base_semitone = system_registry.chord_play.getChordBaseSemitone();
    if (_bass_semitone != base_semitone) {
      _bass_semitone = base_semitone;
      reset_request = true;
    }

    // スロットが切り替わっていれば強制的に先頭に戻すフラグを立てる(強制)
    if (_slot_index != slot_index) {
      _slot_index = slot_index;
      force_reset = true;
      reset_request = true;
    }

    _arpeggio_reset_remain_usec = -1;
  } else {
    auto tempo = system_registry.song_data.song_info.getTempo();
    if (tempo < def::app::tempo_bpm_min) { tempo = def::app::tempo_bpm_default; }
    uint32_t cycle_usec = (60 * 1000 * 1000 + (tempo >> 1)) / tempo;
    _arpeggio_reset_remain_usec = cycle_usec * def::app::arpeggio_reset_timeout_beats;
  }

  int firstStepCounter = 0;
  int enabledCounter = 0;

  auto chord_play = &system_registry.chord_play;
  for (int i = 0; i < def::app::max_chord_part; ++i) {
    if (reset_request) {
      chordNoteOff(i);
    }
    if (!chord_play->getPartEnable(i)) { continue; }
    auto chord_part = &system_registry.current_slot->chord_part[i];
    auto part_info = &chord_part->part_info;
    int end_point = part_info->getLoopStep();
    int step = chord_play->getPartStep(i);
    {
      // ステップを戻す処理
      if (force_reset || step >= end_point || 
        (reset_request && step > part_info->getAnchorStep()))
      {
        step = 0;
        chord_play->setPartStep(i, 0);
        if (!chord_play->getPartNextEnable(i)) {
          chord_play->setPartEnable(i, false);
          chordNoteOff(i);
        }
      }
      if (end_point > 2) {
        firstStepCounter += (bool)(step <= 0);
        ++enabledCounter;
      }
    }
  }

  if (enabledCounter == 0 || ((firstStepCounter << 1) > enabledCounter)) {
    // _step_reset_request = false;
    // ループ先頭に達したとき、パートを有効にする処理を実施
    for (int i = 0; i < def::app::max_chord_part; ++i) {
      bool current_enable = chord_play->getPartEnable(i);
      bool next_enable = chord_play->getPartNextEnable(i);
      if (current_enable == false && next_enable == true) {
        system_registry.chord_play.setPartEnable(i, next_enable);
        system_registry.chord_play.setPartStep(i, 0);
      }
    }

    if (!on_beat) {
      // オートプレイの先頭に戻るタイミングでDegree変更をしようとして間に合わなかった場合
      // 先頭に戻った直後にまたすぐ先頭に戻る…という状況になるため、これを避けるためにここで処置する
      int degree = system_registry.chord_play.getChordDegree();
      if (_degree != degree) {
        // Degreeが変更になった場合は前回のDegreeのボタンを消灯する
        system_registry.working_command.clear( { def::command::chord_degree, _degree } );
        _degree = degree;
        // TODO:ここで音を消すかどうか要検討
        // for (int i = 0; i < def::app::max_chord_part; ++i) {
        //   chordNoteOff(i);
        // }
      }
    }
  }

  for (int i = 0; i < def::app::max_chord_part; ++i) {
    if (!chord_play->getPartEnable(i)) { continue;}
    int step = chord_play->getPartStep(i);

    // オモテ拍・ウラ拍がズレないように調整しつつステップを進める
    if ((bool)(step & 1) == on_beat) {
      ++step;
      chord_play->setPartStep(i, step);
    }
  }
}

// 指定したノートナンバーの音が他のピッチでも鳴っている数を調べる関数
int32_t task_kantanplay_t::checkOtherPitchNote(int part, int pitch, int midi_ch, int note_number)
{
  int count = 0;
  for (int p = 0; p < def::app::max_pitch_with_drum; ++p) {
    if (p == pitch) { continue; }
    for (int m = 0; m < max_manage_history; ++m) {
      auto manage = &_midi_pitch_manage[part][p][m];
      // まだ鳴っていない音や鳴り終わった音は除外する
      if (manage->press_usec >= 0 || manage->release_usec < 0) { continue; }
      if (manage->note_number != note_number || manage->midi_ch != midi_ch) { continue; }
      if (manage->velocity == 0) { continue; }
      ++count;
    }
  }
  // if (count) {
  //   M5_LOGV("checkOtherPitchNote: %d", count);
  // }
  return count;
}


uint32_t task_kantanplay_t::chordProc(void)
{
  uint32_t next_event_timing = INT32_MAX;
  const int progress_usec = (int32_t)(_current_usec - _prev_usec);

  // オートプレイ処理
  if (_auto_play_remain_usec >= 0) {
    bool on_beat = !_is_on_beat;
    auto auto_play = system_registry.runtime_info.getChordAutoplayState();
    if (on_beat && auto_play != def::play::auto_play_mode_t::auto_play_running)
    { // オートプレイモードの停止は裏拍を終えた後に行う
      _auto_play_remain_usec = -1;
    } else {
      int remain_usec = _auto_play_remain_usec - progress_usec;
      if (remain_usec <= 0) {
        _is_on_beat = on_beat;
        chordStep(on_beat);
        chordPlay(on_beat);
        updateTempoSwing();
        remain_usec += on_beat ? _auto_play_onbeat_usec : _auto_play_offbeat_usec;
      }
      _auto_play_remain_usec = remain_usec;
      if (next_event_timing > remain_usec) {
        next_event_timing = remain_usec;
      }
    }
  }

  for (int part = 0; part < def::app::max_chord_part; ++part) {
    bool hit_flg = false;
    for (int pitch = 0; pitch < def::app::max_pitch_with_drum; ++pitch) {
      for (int m = 0; m < max_manage_history; ++m) {
        auto manage = &_midi_pitch_manage[part][pitch][m];

        int press_usec = manage->press_usec;
        if (press_usec >= 0) {
          press_usec -= progress_usec;
          if (press_usec <= 0) {
            press_usec = -1;
            auto note_number = manage->note_number;
            auto midi_ch = manage->midi_ch;
            auto velocity = manage->velocity;
            if (velocity) {
              velocity |= 0x80;
              system_registry.midi_out_control.setNoteVelocity(midi_ch, note_number, 0);
              system_registry.midi_out_control.setNoteVelocity(midi_ch, note_number, velocity);
              hit_flg = true;
            }
          } else if (next_event_timing > press_usec) {
            next_event_timing = press_usec;
          }
          manage->press_usec = press_usec;
        }

        int release_usec = manage->release_usec;
        if (release_usec >= 0) {
          release_usec -= progress_usec;
          if (release_usec <= 0) {
            release_usec = -1;
            auto note_number = manage->note_number;
            auto midi_ch = manage->midi_ch;
            // 同じノートナンバーの音が他のピッチで鳴っていない場合は音を停止する
            if (0 == checkOtherPitchNote(part, pitch, midi_ch, note_number)) {
              system_registry.midi_out_control.setNoteVelocity(manage->midi_ch, manage->note_number, 0);
            }
            manage->note_number = 0xFF;
            manage->velocity = 0;
          } else if (next_event_timing > release_usec) {
            next_event_timing = release_usec;
          }
          manage->release_usec = release_usec;
        }
      }
    }
    if (hit_flg) {
      system_registry.runtime_info.hitPartEffect(part);
    }
  }

  // パターン編集モードでない場合
  if (system_registry.runtime_info.getPlayMode() != def::playmode::playmode_t::chord_edit_mode) {
    // アルペジエータのタイムアウト判定 (一定時間たつと強制的に先頭に戻す)
    if (_arpeggio_reset_remain_usec >= 0) {
      _arpeggio_reset_remain_usec -= progress_usec;
      if (_arpeggio_reset_remain_usec <= 0) {
        _arpeggio_reset_remain_usec = -1;
        for (int i = 0; i < def::app::max_chord_part; ++i) {
          system_registry.chord_play.setPartStep(i, 0);
        }
      } else {
        if (next_event_timing > _arpeggio_reset_remain_usec) {
          next_event_timing = _arpeggio_reset_remain_usec;
        }
      }
    }
  }

  return next_event_timing;
}

void task_kantanplay_t::chordNoteOff(int part)
{
  // auto chord_part = &system_registry.current_slot->chord_part[part];
  for (int pitch_index = 0; pitch_index < def::app::max_pitch_with_drum; ++pitch_index) {
    for (int m = 0; m < max_manage_history; ++m) {
      auto manage = &_midi_pitch_manage[part][pitch_index][m];
      if (manage->press_usec >= 0 || manage->release_usec >= 0) {
        auto note = manage->note_number;
        manage->velocity = 0;
        manage->press_usec = -1;
        manage->release_usec = -1;
        manage->note_number = 0xFF;
        auto midi_ch = manage->midi_ch;
        if (note < def::midi::max_note && midi_ch < def::midi::channel_max) {
          system_registry.midi_out_control.setNoteVelocity(midi_ch, note, 0);
        }
      }
    }
  }
}

void task_kantanplay_t::setPitchManage(uint8_t part, uint8_t pitch, uint8_t midi_ch, uint8_t note_number, int8_t velocity, int32_t press_usec, int32_t release_usec)
{
  auto manage = &_midi_pitch_manage[part][pitch][0];
  { // 履歴末尾のデータが消失する前に、管理している音を停止する
    if (manage[0].press_usec < 0 && manage[0].release_usec >= 0)
    {
      manage[0].release_usec = -1;
      manage[0].press_usec = -1;

      // 同じノートナンバーの音が他のピッチで鳴っていない場合は音を停止する
      if (0 == checkOtherPitchNote(part, pitch, midi_ch, note_number)) {
// M5_LOGV("stop note: %d, pitch: %d, midi_ch: %d, note_number: %d, velocity: %d, press_usec: %d, release_usec: %d", part, pitch, midi_ch, note_number, velocity, press_usec, release_usec);
        system_registry.midi_out_control.setNoteVelocity(midi_ch, note_number, 0);
      }
    }
  }

  // 履歴をずらす
  memmove(&(_midi_pitch_manage[part][pitch][0]), &(_midi_pitch_manage[part][pitch][1]), sizeof(midi_pitch_manage_t) * (max_manage_history - 1));

  // 今回指定された音よりも後のタイミングで処理される予定だった音を探し、予定をキャンセルしたり早めたりする
  for (int m = 0; m < max_manage_history - 1; ++m) {
    if (manage[m].press_usec >= press_usec) {
      manage[m].press_usec = -1;
      manage[m].release_usec = -1;
    } else
    if (manage[m].release_usec > press_usec) {
//M5_LOGV("short note_number: %d, press_usec: %d, release_usec: %d", note_number, press_usec, manage[m].release_usec);
      manage[m].release_usec = press_usec;
    }
  }

  if (velocity < 0 || note_number == 0)
  { // マイナスベロシティやノートナンバー0 は停止処理に変換する
    velocity = 0;
    release_usec = press_usec;
    press_usec = -1;
  }

  if (velocity > 127) { velocity = 127; }

  {
  // M5_LOGV("part: %d, pitch: %d, midi_ch: %d, note_number: %d, velocity: %d, press_usec: %d, release_usec: %d", part, pitch, midi_ch, note_number, velocity, press_usec, release_usec);
    manage[max_manage_history - 1].note_number = note_number;
    manage[max_manage_history - 1].midi_ch = midi_ch;
    manage[max_manage_history - 1].velocity = velocity;
    manage[max_manage_history - 1].release_usec = release_usec;
    manage[max_manage_history - 1].press_usec = press_usec;
  }
}

void task_kantanplay_t::chordPlay(bool on_beat)
{
  int master_key = system_registry.runtime_info.getMasterKey();
  int slot_key = master_key + (int8_t)system_registry.current_slot->slot_info.getKeyOffset();
  while (slot_key < 0) { slot_key += 12; }
  while (slot_key >= 12) { slot_key -= 12; }
  int degree = _degree;  //system_registry.chord_play.getChordDegree();

  // 動作中のコードボタンの表示反映
  if (on_beat) { // オモテ拍のタイミングで有効化
    system_registry.working_command.set( { def::command::chord_degree, _degree } );
  } else {       // ウラ拍のタイミングで解除
    system_registry.working_command.clear( { def::command::chord_degree, _degree } );
  }

  KANTANMusic_GetMidiNoteNumberOptions options;
  KANTANMusic_GetMidiNoteNumber_SetDefaultOptions(&options);
  options.minor_swap = system_registry.chord_play.getChordMinorSwap();
  options.semitone_shift = system_registry.chord_play.getChordSemitone();
  options.modifier = system_registry.chord_play.getChordModifier();
  options.bass_degree = _bass_degree;
  options.bass_semitone_shift = _bass_semitone;

// M5_LOGE("key: %d, minor_swap: %d, modifier: %d, semitone: %d", key, minor_swap, (int)modifier, semitone);
  for (int part = 0; part < def::app::max_chord_part; ++part) {
    bool part_en = system_registry.chord_play.getPartEnable(part);
    uint8_t midi_ch = part;
    auto chord_part = &system_registry.current_slot->chord_part[part];
    auto part_info = &chord_part->part_info;
    options.position = part_info->getPosition();
    options.voicing = part_info->getVoicing();

    int displacement_usec = 1000 * part_info->getStrokeSpeed();
    int autorelease_usec = 1000 * def::app::autorelease_msec;
    int32_t press_usec = 0;
    int step = system_registry.chord_play.getPartStep(part);
    if (step < 0) {
      continue;
    }
    bool flg_use = false;

    int pitch_flow = 1;
    int pitch_index = 0;
    int pitch_last = def::app::max_pitch_with_drum;

    // ドラムパートの場合の処理分岐
    bool is_drum = (part_info->isDrumPart());
    bool mute = false;
    if (is_drum) {
      displacement_usec = 0;
      midi_ch = def::midi::channel_10;
    } else {
      switch (chord_part->arpeggio.getStyle(step))
      {
      default:
      case def::play::arpeggio_style_t::same_time:
        displacement_usec = 0;
        break;

      case def::play::arpeggio_style_t::high_to_low:
        pitch_flow = 1;
        pitch_index = 0;
        pitch_last = def::app::max_pitch_with_drum;
        break;

      case def::play::arpeggio_style_t::low_to_high:
        pitch_flow = -1;
        pitch_index = def::app::max_pitch_with_drum - 1;
        pitch_last = -1;
        break;

      case def::play::arpeggio_style_t::mute:
        mute = true;
        // ミュート処理は同時発音ではなく高速ダウンストロークとして扱う

        // ミュート処理の時はリリースまでの時間はストロークスピードの 2倍とする
        autorelease_usec = displacement_usec * 2;

        // ミュート処理の時はストロークスピードは 1/4 とする。
        displacement_usec >>= 2;
        pitch_flow = -1;
        pitch_index = def::app::max_pitch_with_drum - 1;
        pitch_last = -1;
        break;
      }
    }

    for (; pitch_index != pitch_last; pitch_index += pitch_flow) {
      int velocity = 0;
      if (part_en) { velocity = chord_part->arpeggio.getVelocity(step, pitch_index); }
      if (velocity) {
        if (0 < velocity) {
          velocity = velocity * _press_velocity / 100;
          if (velocity > 127) { velocity = 127; }
          if (velocity < 1) { velocity = 1; }
        }
      } else {
        if (!mute) {
          continue;
        }
        // if (manage->velocity == 0
        //  && manage->midi_ch == midi_ch) {
        //     continue;
        //  }
      }

      uint32_t note = 0;
      if (is_drum) {
        note = system_registry.song_data.chord_part_drum[part].getDrumNoteNumber(pitch_index);
      } else {
        if (pitch_index >= 6) { continue; }
        // M5_LOGV("degree: %d, slot_key: %d, semitone: %d, base_degree:%d, base_semitone:%d", degree, slot_key, options.semitone_shift, options.bass_degree, options.bass_semitone_shift);
        note = KANTANMusic_GetMidiNoteNumber(
          6 - pitch_index
          , degree
          , slot_key
          , &options
        );
      }
      setPitchManage(part, pitch_index, midi_ch, note, velocity, press_usec, press_usec + autorelease_usec);
      press_usec += displacement_usec;
      flg_use = true;
    }
    if (flg_use) {
      uint8_t program = part_info->getTone();
      uint8_t chvolume = part_info->getVolume() * 127 / 100;
      if (chvolume > 127) { chvolume = 127; }
      system_registry.midi_out_control.setProgramChange(midi_ch, program);
      system_registry.midi_out_control.setChannelVolume(midi_ch, chvolume);
    }
  }
}

void task_kantanplay_t::procNoteButton(const def::command::command_param_t& command_param, const bool is_pressed)
{
  const uint8_t button_index = command_param.getParam() - 1;
  if (button_index >= def::hw::max_main_button) { return; }

  const bool on_beat = is_pressed;

  // 押した時と離した時でボタンのマッピングが変化している可能性がある。
  // 押した時点の情報を_midi_note_manageに記憶しておき、次回おなじボタンが操作されたらまず前回のボタンをノートオフする
  auto manage = &_midi_note_manage[button_index];
  auto midi_ch = manage->midi_ch;
  auto note = manage->note_number;
  if (note < 127 && midi_ch < def::midi::channel_max) {
    system_registry.midi_out_control.setNoteVelocity(midi_ch, note, 0);
    manage->note_number = 0xFF;
  }
  if (!on_beat) {
    return;
  }

  int32_t master_key = (int8_t)system_registry.runtime_info.getMasterKey();
  uint8_t note_scale = system_registry.current_slot->slot_info.getNoteScale();
  int32_t offset_key = (int8_t)system_registry.current_slot->slot_info.getKeyOffset();
  note = def::play::note::note_scale_note_table[note_scale][button_index];

  note += master_key + offset_key;
  if (note < 128) {
    // TODO:ボリュームの設定をスロット情報から取得する
    uint8_t volume = 100;
    uint8_t chvolume = volume * 127 / 100;
    if (chvolume > 127) { chvolume = 127; }

    uint8_t program = system_registry.current_slot->slot_info.getNoteProgram();
    midi_ch = def::midi::channel_7;
    manage->midi_ch = midi_ch;
    manage->note_number = note;
    system_registry.midi_out_control.setChannelVolume(midi_ch, chvolume);
    system_registry.midi_out_control.setProgramChange(midi_ch, program);
    system_registry.midi_out_control.setNoteVelocity(midi_ch, note, 0);
    uint8_t velocity = 0x80 | (_press_velocity > 127 ? 127 : _press_velocity);
    system_registry.midi_out_control.setNoteVelocity(midi_ch, note, velocity);
  }
}

void task_kantanplay_t::procDrumButton(const def::command::command_param_t& command_param, const bool is_pressed)
{
  const uint8_t button_index = command_param.getParam() - 1;
  if (button_index >= def::hw::max_main_button) { return; }

  const bool on_beat = is_pressed;
  auto manage = &_midi_note_manage[button_index];

  auto midi_ch = manage->midi_ch;
  auto note = manage->note_number;

  if (note < 127 && midi_ch < def::midi::channel_max) {
    system_registry.midi_out_control.setNoteVelocity(midi_ch, note, 0);
    manage->note_number = 0xFF;
  }
  if (!on_beat) {
    return;
  }

  note = system_registry.drum_mapping.get8(button_index);

  if (note < 128) {
    // TODO:ボリュームの設定をスロット情報から取得する
    uint8_t volume = 100;
    uint8_t chvolume = volume * 127 / 100;
    if (chvolume > 127) { chvolume = 127; }
    midi_ch = def::midi::channel_10;
    manage->midi_ch = midi_ch;
    manage->note_number = note;
    system_registry.midi_out_control.setChannelVolume(midi_ch, chvolume);

    uint8_t velocity = 0x80 | (_press_velocity > 127 ? 127 : _press_velocity);
    system_registry.midi_out_control.setNoteVelocity(def::midi::channel_10, note, velocity);
  }
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns
