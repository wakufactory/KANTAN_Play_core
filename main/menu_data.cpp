#include <M5Unified.h>

#include "menu_data.hpp"
#include "file_manage.hpp"

namespace kanplay_ns {
//-------------------------------------------------------------------------
// extern instance
menu_control_t menu_control;

static std::string _title_text_buffer;

static menu_item_ptr_array getMenuArray(def::menu_category_t category);

// 指定したメニューの直属の親階層のインデックスを取得する
static uint8_t getParentIndex(const menu_item_ptr_array &menu, uint8_t child_index)
{
  uint8_t target_level = menu[child_index]->getLevel();
  for (uint8_t i = child_index; i > 0; --i) {
    if (menu[i]->getLevel() < target_level) {
      return i;
    }
  }
  return 0;
}
/*
// 指定したメニューの直下の子階層インデックスリストを取得する
static size_t getSubMenuIndexList(uint8_t* index_list, const menu_item_ptr_array &menu, size_t size, uint8_t parent_index)
{
  size_t i = 0;
  // 親階層よりひとつ深い階層のメニューを探索ターゲットとする
  auto target_level = 1 + menu[parent_index]->getLevel();
  for (size_t j = parent_index + 1; menu[j] != nullptr; ++j) {
    // 目的の階層より浅い階層のメニューが見つかったら終了
    if (menu[j]->getLevel() < target_level) { break; }
    // 目的の階層より深い階層のメニューは無視
    if (menu[j]->getLevel() > target_level) { continue; }
    index_list[i++] = j;
    if (i >= size) { break; }
// M5_LOGV("getSubMenuIndexList: %d", j);
  }
  // 取得した数を返す
  return i;
}
//*/
// 指定したメニューの直下の子階層インデックスリストを取得する
static int getSubMenuIndexList(std::vector<uint16_t> *index_list, const menu_item_ptr_array &menu, uint8_t parent_index)
{
  int result = 0;
  // 親階層よりひとつ深い階層のメニューを探索ターゲットとする
  auto target_level = 1 + menu[parent_index]->getLevel();
  for (size_t j = parent_index + 1; menu[j] != nullptr; ++j) {
    // 目的の階層より浅い階層のメニューが見つかったら終了
    if (menu[j]->getLevel() < target_level) { break; }
    // 目的の階層より深い階層のメニューは無視
    if (menu[j]->getLevel() > target_level) { continue; }
    ++result;
    if (index_list != nullptr)
    {
      index_list->push_back(j);
    }
  }
  // 取得した数を返す
  return result;
}

bool menu_item_t::exit(void) const
{
  if (_seq == 0) {
    return false;
  }
  auto array = getMenuArray(_category);

  uint8_t parent_index = getParentIndex(array, _seq);
  auto level = array[parent_index]->getLevel();
  system_registry.menu_status.setCurrentLevel(level);
  system_registry.menu_status.setCurrentSequence(parent_index);
  return true;
}

bool menu_item_t::enter(void) const
{
  auto array = getMenuArray(_category);

  system_registry.menu_status.setSelectIndex(_level - 1, _seq);
  system_registry.menu_status.setCurrentLevel(_level);
  system_registry.menu_status.setCurrentSequence(_seq);
  if (array[_seq + 1] != nullptr && _level + 1 == array[_seq + 1]->getLevel()) {
    system_registry.menu_status.setSelectIndex(_level, _seq + 1);
    return true;
  }
  system_registry.menu_status.setSelectIndex(_level, _seq);
  return false;
}

struct mi_tree_t : public menu_item_t {
  constexpr mi_tree_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : menu_item_t { cate, seq, level, title } {}

  menu_item_type_t getType(void) const override { return menu_item_type_t::mt_tree; }

  size_t getSelectorCount(void) const override {
    auto array = getMenuArray(_category);

    return getSubMenuIndexList(nullptr, array, _seq);
  }

  bool inputNumber(uint8_t number) const override
  {
    size_t cursor_pos = number - getMinValue();

    auto array = getMenuArray(_category);

    std::vector<uint16_t> child_list;
    auto child_count = getSubMenuIndexList(&child_list, array, _seq);

    if (cursor_pos < child_count) {
      int enter_index = child_list[cursor_pos];
      auto item = array[enter_index];
      auto level = item->getLevel();
      system_registry.menu_status.setSelectIndex(level - 1, enter_index);

      // 数字を押した時点ではサブメニューに入らない
      return true;

      // サブメニューに入る場合はこちら
      // return array[enter_index]->enter();
    }
    return false;
  }

  bool inputUpDown(int updown) const override
  {
    auto array = getMenuArray(_category);

    std::vector<uint16_t> child_list;
    auto child_count = getSubMenuIndexList(&child_list, array, _seq);

    if (!child_count) { return false; }

    int level = system_registry.menu_status.getCurrentLevel();
    int focus_index = system_registry.menu_status.getSelectIndex(level);

    auto list_position = 0;
    for (int i = 0; i < child_count; ++i) {
      if (child_list[i] == focus_index) {
        list_position = i;
        break;
      }
    }

    list_position += updown;
    if (list_position > child_count - 1) {
      list_position = child_count - 1;
    }
    if (list_position < 0) {
      list_position = 0;
    }
    focus_index = child_list[list_position];
    system_registry.menu_status.setSelectIndex(level, focus_index);

    return true;
  }

protected:
};

struct mi_normal_t : public menu_item_t {
  constexpr mi_normal_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : menu_item_t { cate, seq, level, title } {}

  menu_item_type_t getType(void) const override { return menu_item_type_t::mt_normal; }

  size_t getSelectorCount(void) const override { return getMaxValue() - getMinValue() + 1; }

  bool enter(void) const override {
    _input_number_result = 0;
    _selecting_value = getValue();
    auto min_value = getMinValue();
    if (_selecting_value < min_value) { _selecting_value = min_value; }
    return menu_item_t::enter();
  }

  bool execute(void) const override
  {
    if (!setValue(_selecting_value)) { return false; }
    exit();
    return true;
  }

  int getSelectingValue(void) const override
  {
    return _selecting_value;
  }

  bool setSelectingValue(int value) const override
  {
    bool result = true;
    auto min_value = getMinValue();
    if (value < min_value) { value = min_value; result = false; }
    auto max_value = getMaxValue();
    if (value > max_value) { value = max_value; result = false; }
    _selecting_value = value;
    return result;
  }

  bool inputUpDown(int updown) const override
  {
    return setSelectingValue(_selecting_value + updown);
  }

  bool inputNumber(uint8_t number) const override
  {
    int tmp = (_input_number_result * 10) + number;
    int max_value = getMaxValue();

    if (tmp > max_value)
    {
      auto div = 10000;
      if      (max_value < 10) { div = 10;}
      else if (max_value < 100) { div = 100; }
      else if (max_value < 1000) { div = 1000; }
      tmp %= div;
    }

    _input_number_result = tmp;
    return setSelectingValue(tmp);
  }


protected:
  static int _input_number_result;
  static int _selecting_value;
};
int mi_normal_t::_input_number_result = 0;
int mi_normal_t::_selecting_value = 0;


struct mi_selector_t : public mi_normal_t {
  constexpr mi_selector_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, const text_array_t* names)
  : mi_normal_t { cate, seq, level, title }
  , _names { names }
  {}

  const char* getSelectorText(size_t index) const override { return _names->at(index)->get(); }
  size_t getSelectorCount(void) const override { return _names->size(); }

  const char* getValueText(void) const override
  {
    return _names->at(getValue() - getMinValue())->get();
  }

protected:
  const text_array_t* _names;
};

struct mi_language_t : public mi_selector_t {
protected:
  static constexpr const localize_text_array_t name_array = { 2, (const localize_text_t[]){
    { "English", "English" },
    { "日本語", "日本語" },
  }};

public:
  constexpr mi_language_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array } {}

  int getValue(void) const override
  {
    return getMinValue() + static_cast<uint8_t>(system_registry.user_setting.getLanguage());
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    auto lang = static_cast<def::lang::language_t>(value - getMinValue());
    system_registry.user_setting.setLanguage(lang);
    return true;
  }
};


struct mi_imu_velocity_t : public mi_selector_t {
protected:
  static constexpr const localize_text_array_t name_array = { 3, (const localize_text_t[]){
    { "Disable", "無効" },
    { "Normal",  "標準" },
    { "Strong",  "強め" },
  }};

public:
  constexpr mi_imu_velocity_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array } {}

  int getValue(void) const override
  {
    return getMinValue() + system_registry.user_setting.getImuVelocityLevel();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.user_setting.setImuVelocityLevel(value);
    return true;
  }
};

struct mi_brightness_t : public mi_selector_t {
protected:
  static constexpr const localize_text_array_t name_array = { 5, (const localize_text_t[]){
    { "Very Low",  "最暗" },     // Level 1
    { "Low",      "暗め" },     // Level 2
    { "Medium",   "標準" },     // Level 3
    { "High",     "明るめ" },   // Level 4
    { "Very High", "最明" },     // Level 5
  }};

public:
  constexpr mi_brightness_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array } {}
};

struct mi_lcd_backlight_t : public mi_brightness_t {
public:
  constexpr mi_lcd_backlight_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_brightness_t { cate, seq, level, title } {}
  int getValue(void) const override
  {
    return getMinValue() + system_registry.user_setting.getDisplayBrightness();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.user_setting.setDisplayBrightness(value);
    return true;
  }
};

struct mi_led_brightness_t : public mi_brightness_t {
public:
  constexpr mi_led_brightness_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_brightness_t { cate, seq, level, title } {}
  int getValue(void) const override
  {
    return getMinValue() + system_registry.user_setting.getLedBrightness();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.user_setting.setLedBrightness(value);
    system_registry.rgbled_control.refresh();
    return true;
  }
};

struct mi_vol_midi_t : public mi_normal_t {
  constexpr mi_vol_midi_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title }
  {}
protected:
  int getMinValue(void) const override { return 10; }
  int getMaxValue(void) const override { return 127; }

  int getValue(void) const override
  {
    return system_registry.user_setting.getMIDIMasterVolume();
  }
  bool setValue(int value) const override
  {
    if (mi_normal_t::setValue(value) == false) { return false; }
    system_registry.user_setting.setMIDIMasterVolume(value);
    return true;
  }
  const char* getSelectorText(size_t index) const override {
    int tmp = index + getMinValue();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", tmp);
    _title_text_buffer = buf;
    return _title_text_buffer.c_str();
  }
  const char* getValueText(void) const override
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", getValue());
    _title_text_buffer = buf;
    return _title_text_buffer.c_str();
  }
};

struct mi_vol_adcmic_t : public mi_normal_t {
  constexpr mi_vol_adcmic_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title }
  {}
protected:
  int getMinValue(void) const override { return 0; }
  int getMaxValue(void) const override { return 11; }

  int getValue(void) const override
  {
    return system_registry.user_setting.getADCMicAmp();
  }
  bool setValue(int value) const override
  {
    if (mi_normal_t::setValue(value) == false) { return false; }
    system_registry.user_setting.setADCMicAmp(value);
    return true;
  }
  const char* getSelectorText(size_t index) const override {
    int tmp = index + getMinValue();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", tmp);
    _title_text_buffer = buf;
    return _title_text_buffer.c_str();
  }
  const char* getValueText(void) const override
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", getValue());
    _title_text_buffer = buf;
    return _title_text_buffer.c_str();
  }
};

struct mi_detail_view_t : public mi_selector_t {
protected:
  static constexpr const localize_text_array_t name_array = { 2, (const localize_text_t[]){
    { "icon view"  , "アイコン表示" },
    { "detail view", "詳細表示"     },
  }};

public:
  constexpr mi_detail_view_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array } {}

  int getValue(void) const override
  {
    return getMinValue() + system_registry.user_setting.getGuiDetailMode();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.user_setting.setGuiDetailMode(value);
    return true;
  }
};

struct mi_enable_selector_t : public mi_selector_t {
protected:
  static constexpr const localize_text_array_t name_array = { 2, (const localize_text_t[]){
    { "Off",     "オフ"  },
    { "On",      "オン"  },
  }};

public:
  constexpr mi_enable_selector_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array } {}
};

struct mi_wave_view_t : public mi_enable_selector_t {
  public:
    constexpr mi_wave_view_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
    : mi_enable_selector_t { cate, seq, level, title } {}
  
    int getValue(void) const override
    {
      return getMinValue() + static_cast<uint8_t>(system_registry.user_setting.getGuiWaveView());
    }
    bool setValue(int value) const override
    {
      if (mi_selector_t::setValue(value) == false) { return false; }
      value -= getMinValue();
      system_registry.user_setting.setGuiWaveView(value);
      return true;
    }
  };


struct mi_usewifi_t : public mi_enable_selector_t {
public:
  constexpr mi_usewifi_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_enable_selector_t { cate, seq, level, title } {}

  int getValue(void) const override
  {
    return getMinValue() + static_cast<uint8_t>(system_registry.wifi_control.getMode());
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.wifi_control.setMode(static_cast<def::command::wifi_mode_t>(value));
    return true;
  }
};

struct mi_all_reset_t : public mi_selector_t {
protected:
  static constexpr const localize_text_array_t name_array = { 2, (const localize_text_t[]){
    { "Cancel", "キャンセル" },
    { "Reset",  "リセット"   },
  }};

public:
  constexpr mi_all_reset_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array } {}

  const char* getValueText(void) const override { return "..."; }

  int getValue(void) const override
  {
    return getMinValue();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    if (value == 1) {
      system_registry.reset();
      system_registry.save();
      system_registry.popup_notify.setPopup(true, def::notify_type_t::NOTIFY_ALL_RESET);
    }
    return true;
  }
};

  
#if 0
struct mi_intvalue_t : public mi_normal_t {
  constexpr mi_intvalue_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, const int16_t min_value, const int16_t max_value, const int16_t step)
  : mi_normal_t { cate, seq, level, title }
  , _min_value { min_value }
  , _max_value { max_value }
  , _step { step }
  {}
  virtual menu_item_type_t getType(void) const { return menu_item_type_t::input_number; }

  const char* getValueText(void) const override
  {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d", getValue());
    return buf;
  }

  bool inputUpDown(int updown) const override
  {
    int value = getValue();
    value += updown * _step;
    if (value < _min_value) { value = _min_value; }
    if (value > _max_value) { value = _max_value; }
    return setValue(value);
  }

  bool inputNumber(uint8_t value) const override
  {
    if (value < _min_value) { value = _min_value; }
    if (value > _max_value) { value = _max_value; }
    return setValue(value);
  }

protected:
  int16_t _min_value;
  int16_t _max_value;
  int16_t _step;
};
#endif

struct mi_program_t : public mi_selector_t {
  constexpr mi_program_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &def::midi::program_name_table }
  {}
protected:
  int getValue(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    return system_registry.current_slot->chord_part[part_index].part_info.getTone() + getMinValue();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    auto part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.current_slot->chord_part[part_index].part_info.setTone(value);
    return true;
  }
};

struct mi_octave_t : public mi_normal_t {
  constexpr mi_octave_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title }
  {}
protected:
  const char* getSelectorText(size_t index) const override {
    return def::app::position_name_table.at(index * 4)->get();
  }
  size_t getSelectorCount(void) const override { return (def::app::position_name_table.size() >> 2) + 1; }

  const char* getValueText(void) const override
  {
    return def::app::position_name_table.at( (getValue() - getMinValue()) << 2 )->get();
  }

  int getValue(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    return (system_registry.current_slot->chord_part[part_index].part_info.getPosition() >> 2) + 10;
  }
  bool setValue(int value) const override
  {
    if (mi_normal_t::setValue(value) == false) { return false; }
    int v = (value - 10) << 2;
    auto part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.current_slot->chord_part[part_index].part_info.setPosition(v);
    return true;
  }
};

struct mi_voicing_t : public mi_normal_t {
  constexpr mi_voicing_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title }
  {}
protected:
  const char* getSelectorText(size_t index) const override {
    return KANTANMusic_GetVoicingName(static_cast<KANTANMusic_Voicing>(index));
  }
  size_t getSelectorCount(void) const override { return KANTANMusic_Voicing::KANTANMusic_MAX_VOICING; }

  const char* getValueText(void) const override
  {
    return KANTANMusic_GetVoicingName(static_cast<KANTANMusic_Voicing>(getValue() - getMinValue()));
  }

  int getValue(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    return system_registry.current_slot->chord_part[part_index].part_info.getVoicing() + getMinValue();
  }
  bool setValue(int value) const override
  {
    if (mi_normal_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    auto part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.current_slot->chord_part[part_index].part_info.setVoicing(value);
    return true;
  }
};

struct mi_clear_notes_t : public mi_normal_t {
  constexpr mi_clear_notes_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title }
  {}
protected:
  const char* getValueText(void) const override { return "..."; }
  const char* getSelectorText(size_t index) const override { return "Clear All Notes"; }

  bool execute(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.current_slot->chord_part[part_index].arpeggio.reset();
    system_registry.popup_notify.setPopup(true, def::notify_type_t::NOTIFY_CLEAR_ALL_NOTES);
    return mi_normal_t::execute();
  }

  size_t getSelectorCount(void) const override { return 1; }

  int getValue(void) const override
  {
    return 0;
  }
  bool setValue(int value) const override
  {
    return true;
  }
};

struct mi_percent_t : public mi_selector_t {
  static constexpr const simple_text_array_t name_array = { 20, (const simple_text_t[]){
     "5%",  "10%",  "15%",  "20%",
    "25%",  "30%",  "35%",  "40%",
    "45%",  "50%",  "55%",  "60%",
    "65%",  "70%",  "75%",  "80%",
    "85%",  "90%",  "95%", "100%",
  }};

  constexpr mi_percent_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array }
  {}
};

struct mi_partvolume_t : public mi_percent_t {
  constexpr mi_partvolume_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_percent_t { cate, seq, level, title}
  {}
protected:
  int getValue(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    return system_registry.current_slot->chord_part[part_index].part_info.getVolume() / 5;
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    auto part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.current_slot->chord_part[part_index].part_info.setVolume(value * 5);
    return true;
  }
};

struct mi_velocity_t : public mi_selector_t {
  static constexpr const simple_text_array_t name_array = { 21, (const simple_text_t[]){
    "mute",
      "5%",  "10%",  "15%",  "20%",
    "25%",  "30%",  "35%",  "40%",
    "45%",  "50%",  "55%",  "60%",
    "65%",  "70%",  "75%",  "80%",
    "85%",  "90%",  "95%", "100%",
  }};

  constexpr mi_velocity_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array }
  {}

protected:
  int getValue(void) const override
  {
    int velo = system_registry.runtime_info.getEditVelocity();
    if (velo < 0) return 1;
    return 1 + (velo / 5);
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    int velo = (value == 1) ? -5 : (value - 1) * 5;
    system_registry.runtime_info.setEditVelocity(velo);
    return true;
  }
};

struct mi_arpeggio_step_t : public mi_selector_t {
  static constexpr const simple_text_array_t name_array = { 16, (const simple_text_t[]){
    "1",  "2",  "3",  "4",
    "5",  "6",  "7",  "8",
    "9", "10", "11", "12",
   "13", "14", "15", "16",
  }};

  constexpr mi_arpeggio_step_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array }
  {}
};

struct mi_loop_length_t : public mi_arpeggio_step_t {
  constexpr mi_loop_length_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_arpeggio_step_t { cate, seq, level, title }
  {}
protected:
  int getValue(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    return system_registry.current_slot->chord_part[part_index].part_info.getLoopStep() / 2 + 1;
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    auto part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.current_slot->chord_part[part_index].part_info.setLoopStep(value * 2 - 1);
    return true;
  }
};

struct mi_anchor_step_t : public mi_arpeggio_step_t {
  constexpr mi_anchor_step_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_arpeggio_step_t { cate, seq, level, title }
  {}
protected:
  int getValue(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    return system_registry.current_slot->chord_part[part_index].part_info.getAnchorStep() / 2 + 1;
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    auto part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.current_slot->chord_part[part_index].part_info.setAnchorStep(value * 2 - 2);
    return true;
  }
};

struct mi_stroke_speed_t : public mi_selector_t {
  static constexpr const simple_text_array_t name_array = { 10, (const simple_text_t[]){
    "5 msec",  "10 msec",  "15 msec",  "20 msec",
    "25 msec", "30 msec",  "35 msec",  "40 msec",
    "45 msec", "50 msec"
  }};

  constexpr mi_stroke_speed_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array }
  {}
protected:
  int getValue(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    return system_registry.current_slot->chord_part[part_index].part_info.getStrokeSpeed() / 5;
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    auto part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.current_slot->chord_part[part_index].part_info.setStrokeSpeed(value * 5);
    return true;
  }
};

struct mi_slot_playmode_t : public mi_selector_t {
  static constexpr const localize_text_array_t name_array = { 3, (const localize_text_t[]){
    { "Chord Mode", "コード" },
    { "Note Mode",  "ノート" },
    { "Drum Mode",  "ドラム" },
  }};

  constexpr mi_slot_playmode_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array }
  {}

  int getValue(void) const override
  {
    return system_registry.current_slot->slot_info.getPlayMode();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    auto mode = def::playmode::playmode_t::chord_mode;
    switch (value) {
    default: 
    case 1: break;
    case 2: mode = def::playmode::playmode_t::note_mode; break;
    case 3: mode = def::playmode::playmode_t::drum_mode; break;
    }
    system_registry.operator_command.addQueue({ def::command::play_mode_set, mode });
    // system_registry.current_slot->slot_info.setPlayMode(mode);
    return true;
  }
};

struct mi_slot_key_t : public mi_selector_t {
  static constexpr const simple_text_array_t name_array = { 23, (const simple_text_t[]){
    "- 11","- 10","- 9", " -8",
    "- 7", "- 6", "- 5", " -4",
    "- 3", "- 2", "- 1", "± 0",
    "+ 1", "+ 2", "+ 3", "+ 4",
    "+ 5", "+ 6", "+ 7", "+ 8",
    "+ 9", "+ 10","+ 11"
  }};

  constexpr mi_slot_key_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array }
  {}

  int getValue(void) const override
  {
    auto key_offset = system_registry.current_slot->slot_info.getKeyOffset();
    return key_offset + 12;
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    auto key_offset = value - 12;
    system_registry.current_slot->slot_info.setKeyOffset(key_offset);
    return true;
  }
};

struct mi_slot_clipboard_t : public mi_selector_t {
  static constexpr const localize_text_array_t name_array = { 2, (const localize_text_t[]){
    { "Copy Setting"  , "設定コピー" },
    { "Paste Setting" , "設定ペースト" },
  }};

  const char* getValueText(void) const override { return "..."; }

  constexpr mi_slot_clipboard_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array }
  {}

  bool execute(void) const override
  {
    // auto part_index = system_registry.chord_play.getEditTargetPart();
    // auto slot_index = system_registry.runtime_info.getPlaySlot();
    // auto slot = &system_registry.song_data.slot[slot_index];
    switch (getSelectingValue()) {
    case 1:
      system_registry.clipboard_slot.assign(*system_registry.current_slot);
      system_registry.popup_notify.setPopup(true, def::notify_type_t::NOTIFY_COPY_SLOT_SETTING);
      system_registry.clipboard_content = system_registry_t::clipboard_contetn_t::CLIPBOARD_CONTENT_SLOT;
      // M5_LOGV("mi_slot_clipboard_t: Copy Setting");
      break;

    case 2:
      {
        bool flg = (system_registry.clipboard_content == system_registry_t::clipboard_contetn_t::CLIPBOARD_CONTENT_SLOT);
        if (flg) {
          system_registry.current_slot->assign(system_registry.clipboard_slot);
        }
        system_registry.popup_notify.setPopup(flg, def::notify_type_t::NOTIFY_PASTE_SLOT_SETTING);
      }
      break;

    default:
      // M5_LOGV("mi_slot_clipboard_t: unknown: %d", getValue());
      return false;
    }
    return mi_selector_t::execute();
  }
};

struct mi_part_clipboard_t : public mi_selector_t {
  static constexpr const localize_text_array_t name_array = { 2, (const localize_text_t[]){
    { "Copy Part"    , "パートコピー" },
    { "Paste Part"   , "パートペースト" },
  }};

  const char* getValueText(void) const override { return "..."; }

  constexpr mi_part_clipboard_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array }
  {}

  bool execute(void) const override
  {
    auto part_index = system_registry.chord_play.getEditTargetPart();
    switch (getSelectingValue()) {
    case 1:
      system_registry.clipboard_slot.chord_part[0].assign(system_registry.current_slot->chord_part[part_index]);
      system_registry.popup_notify.setPopup(true, def::notify_type_t::NOTIFY_COPY_PART_SETTING);
      system_registry.clipboard_content = system_registry_t::clipboard_contetn_t::CLIPBOARD_CONTENT_PART;
      break;

    case 2:
      {
        bool flg = (system_registry.clipboard_content == system_registry_t::clipboard_contetn_t::CLIPBOARD_CONTENT_PART);
        if (flg) {
          system_registry.current_slot->chord_part[part_index].assign(system_registry.clipboard_slot.chord_part[0]);
        }
        system_registry.popup_notify.setPopup(flg, def::notify_type_t::NOTIFY_PASTE_PART_SETTING);
      }
      break;

    default:
      // M5_LOGV("mi_part_clipboard_t: unknown: %d", getValue());
      return false;
    }
    return mi_selector_t::execute();
  }
};

struct mi_song_tempo_t : public mi_normal_t {
  constexpr mi_song_tempo_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title }
  {}
protected:
  int getMinValue(void) const override { return def::app::tempo_bpm_min; }
  int getMaxValue(void) const override { return def::app::tempo_bpm_max; }
  // size_t getSelectorCount(void) const override { return def::app::tempo_bpm_max - def::app::tempo_bpm_min + 1; }

  int getValue(void) const override
  {
    return system_registry.song_data.song_info.getTempo();
    // return system_registry.current_slot->slot_info.getTempo();
  }
  bool setValue(int value) const override
  {
    if (mi_normal_t::setValue(value) == false) { return false; }
    system_registry.song_data.song_info.setTempo(value);
/*
    system_registry.current_slot->slot_info.setTempo(value);
    for (int i = 0; i < def::app::max_slot; ++i) {
      system_registry.song_data.slot[i].slot_info.setTempo(value);
    }
*/
    return true;
  }
  const char* getSelectorText(size_t index) const override {
    int tempo = index + getMinValue();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d bpm", tempo);
    _title_text_buffer = buf;
    return _title_text_buffer.c_str();
  }
  const char* getValueText(void) const override
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d bpm", getValue());
    _title_text_buffer = buf;
    return _title_text_buffer.c_str();
  }
};

struct mi_song_swing_t : public mi_normal_t {
  constexpr mi_song_swing_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title }
  {}
protected:
  int getMinValue(void) const override { return def::app::swing_percent_min; }
  int getMaxValue(void) const override { return def::app::swing_percent_max / 10; }

  int getValue(void) const override
  {
    return system_registry.song_data.song_info.getSwing() / 10;
    // return system_registry.current_slot->slot_info.getSwing();
  }
  bool setValue(int value) const override
  {
    if (mi_normal_t::setValue(value) == false) { return false; }
    system_registry.song_data.song_info.setSwing(value * 10);
/*
    system_registry.current_slot->slot_info.setSwing(value);
    for (int i = 0; i < def::app::max_slot; ++i) {
      system_registry.song_data.slot[i].slot_info.setSwing(value);
    }
*/
    return true;
  }
  const char* getSelectorText(size_t index) const override {
    int sw = index * 10;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %%", sw);
    _title_text_buffer = buf;
    return _title_text_buffer.c_str();
  }
  const char* getValueText(void) const override
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %%", getValue() * 10);
    _title_text_buffer = buf;
    return _title_text_buffer.c_str();
  }
};

struct mi_drum_note_t : public mi_selector_t {
  constexpr mi_drum_note_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, uint8_t pitch_number )
  : mi_selector_t { cate, seq, level, title, &def::midi::drum_note_name_tbl } // 35 = Acoustic Bass Drum, 81 = Open Triangle
  , _pitch_number { pitch_number }
  {}
protected:
  const uint8_t _pitch_number;

  // 設定可能な最小値を取得する
  int getMinValue(void) const override { return def::midi::drum_note_name_min; }

  int getValue(void) const override
  {
    int part_index = system_registry.chord_play.getEditTargetPart();
    return system_registry.song_data.chord_part_drum[part_index].getDrumNoteNumber(_pitch_number);
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    int part_index = system_registry.chord_play.getEditTargetPart();
    system_registry.song_data.chord_part_drum[part_index].setDrumNoteNumber(_pitch_number, value);
    return true;
  }
};

struct mi_modifier_t : public mi_selector_t {
protected:
  const uint8_t _button_index;
  static constexpr const simple_text_array_t name_array = { 14, (const simple_text_t[]){
    "dim",
    "m7_5",
    "sus4",
    "6",
    "7",
    "Add9",
    "M7",
    "aug",
    "7sus4",
    "dim7",
    "〜",
    "♭",
    "♯",
    "---",
  }};
  static constexpr const def::command::command_param_array_t listitem[] = {
    { def::command::chord_modifier, KANTANMusic_Modifier_dim },
    { def::command::chord_modifier, KANTANMusic_Modifier_m7_5 },
    { def::command::chord_modifier, KANTANMusic_Modifier_sus4 },
    { def::command::chord_modifier, KANTANMusic_Modifier_6 },
    { def::command::chord_modifier, KANTANMusic_Modifier_7 },
    { def::command::chord_modifier, KANTANMusic_Modifier_Add9 },
    { def::command::chord_modifier, KANTANMusic_Modifier_M7 },
    { def::command::chord_modifier, KANTANMusic_Modifier_aug },
    { def::command::chord_modifier, KANTANMusic_Modifier_7sus4 },
    { def::command::chord_modifier, KANTANMusic_Modifier_dim7 },
    { def::command::chord_minor_swap, 1 },
    { def::command::chord_semitone, 1 },
    { def::command::chord_semitone, 2 },
    { def::command::none }
  };
public:
  constexpr mi_modifier_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, uint8_t button_index)
  : mi_selector_t { cate, seq, level, title, &name_array }
  , _button_index { button_index } {}

  int getValue(void) const override
  {
    auto cmd = system_registry.command_mapping_custom_main.getCommandParamArray(_button_index);
    int index = 0;
    for (int i = 0; ; ++i) {
      auto c = listitem[i];
      if (c == cmd) {
        return getMinValue() + i;
      }
      if (c.array[0].command == def::command::none) { break; }
    }
    return getMinValue();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.command_mapping_custom_main.setCommandParamArray(_button_index, listitem[value]);
    return true;
  }
};


struct mi_ext_assign_t : public mi_selector_t {
  protected:
    const uint8_t _button_index;
    static constexpr const localize_text_array_t name_array = { 124, (const localize_text_t[]) {
      { "Play_Button_01", "プレイボタン01" }, { "Play_Button_02", "プレイボタン02" }, { "Play_Button_03", "プレイボタン03" }, { "Play_Button_04", "プレイボタン04" }, { "Play_Button_05", "プレイボタン05" },
      { "Play_Button_06", "プレイボタン06" }, { "Play_Button_07", "プレイボタン07" }, { "Play_Button_08", "プレイボタン08" }, { "Play_Button_09", "プレイボタン09" }, { "Play_Button_10", "プレイボタン10" },
      { "Play_Button_11", "プレイボタン11" }, { "Play_Button_12", "プレイボタン12" }, { "Play_Button_13", "プレイボタン13" }, { "Play_Button_14", "プレイボタン14" }, { "Play_Button_15", "プレイボタン15" },
      { "Slot_Button_1",  "スロットボタン1" }, { "Slot_Button_2",  "スロットボタン2" }, { "Slot_Button_3",  "スロットボタン3" }, { "Slot_Button_4",  "スロットボタン4" },
      { "Slot_Button_5",  "スロットボタン5" }, { "Slot_Button_6",  "スロットボタン6" }, { "Slot_Button_7",  "スロットボタン7" }, { "Slot_Button_8",  "スロットボタン8" },
      { "Side_Button_L",  "左サイドボタン" }, { "Side_Button_R",  "右サイドボタン" },
      { "Lever_Down",     "シフトレバー下" }, { "Lever_Up",       "シフトレバー上" }, { "Lever_Push",     "シフトレバー押す" },
      { "Dial_1_Left",    "上ダイヤル左回転" }, { "Dial_1_Right",   "上ダイヤル右回転" }, { "Dial_1_Push",    "上ダイヤル押す" },
      { "Dial_2_Left",    "下ダイヤル左回転" }, { "Dial_2_Right",   "下ダイヤル右回転" }, { "Dial_2_Push",    "下ダイヤル押す" },
      { "Wheel_Left",     "ジョグダイヤル左回転" }, { "Wheel_Right",    "ジョグダイヤル右回転" },

      { "1"  , "1"   },
      { "2♭", "2♭" },
      { "2"  , "2"   },
      { "3♭", "3♭" },
      { "3"  , "3"   },
      { "4"  , "4"   },
      { "5♭", "5♭" },
      { "5"  , "5"   },
      { "6♭", "6♭" },
      { "6"  , "6"   },
      { "7♭", "7♭" },
      { "7"  , "7"   },

      { "1〜"  , "1〜"   },
      { "2♭〜", "2♭〜" },
      { "2〜"  , "2〜"   },
      { "3♭〜", "3♭〜" },
      { "3〜"  , "3〜"   },
      { "4〜"  , "4〜"   },
      { "5♭〜", "5♭〜" },
      { "5〜"  , "5〜"   },
      { "6♭〜", "6♭〜" },
      { "6〜"  , "6〜"   },
      { "7♭〜", "7♭〜" },
      { "7〜"  , "7〜"   },

      { "1 [ 7 ]",    "1 [ 7 ]"    },
      { "1 [ M7 ]",   "1 [ M7 ]"   },
      { "2 [ 7 ]",    "2 [ 7 ]"    },
      { "3 [ 7 ]",    "3 [ 7 ]"    },
      { "3〜[ 7 ]",   "3〜[ 7 ]"   },
      { "4 [ 7 ]",    "4 [ 7 ]"    },
      { "4 [ M7 ]",   "4 [ M7 ]"   },
      { "5 [ 7 ]",    "5 [ 7 ]"    },
      { "6 [ 7 ]",    "6 [ 7 ]"    },
      { "7 [ 7 ]",    "7 [ 7 ]"    },
      { "7〜[ 7 ]",   "7〜[ 7 ]"   },
      { "7 [ m7-5 ]", "7 [ m7-5 ]" },
      { "7 [ dim ]",  "7 [ dim ]"  },

      { "[ dim ]",   "[ dim ]"   },
      { "[ m7-5 ]",  "[ m7-5 ]"  },
      { "[ sus4 ]",  "[ sus4 ]"  },
      { "[ 6 ]",     "[ 6 ]"     },
      { "[ 7 ]",     "[ 7 ]"     },
      { "[ Add9 ]",  "[ Add9 ]"  },
      { "[ M7 ]",    "[ M7 ]"    },
      { "[ aug ]",   "[ aug ]"   },
      { "[ 7sus4 ]", "[ 7sus4 ]" },
      { "[ dim7 ]",  "[ dim7 ]"  },
      { "〜",     "〜" },
      { "♭",     "♭" },
      { "♯",     "♯" },

      { "/1"  , "/1"   },
      { "/2♭", "/2♭" },
      { "/2"  , "/2"   },
      { "/3♭", "/3♭" },
      { "/3"  , "/3"   },
      { "/4"  , "/4"   },
      { "/5♭", "/5♭" },
      { "/5"  , "/5"   },
      { "/6♭", "/6♭" },
      { "/6"  , "/6"   },
      { "/7♭", "/7♭" },
      { "/7"  , "/7"   },

      { "〜[ 7 ]",     "〜[ 7 ]"     },
      { "♭〜",        "♭〜"        },
      { "♭〜[ 7 ]",   "♭〜[ 7 ]"   },
      { "♭ [ dim ]",  "♭ [ dim ]"  },
      { "♯ [ dim ]",  "♯ [ dim ]"  },
      { "♭ [ m7-5 ]", "♭ [ m7-5 ]" },
      { "♯ [ m7-5 ]", "♯ [ m7-5 ]" },

      { "Part 1 ON", "パート1 ON" },
      { "Part 2 ON", "パート2 ON" },
      { "Part 3 ON", "パート3 ON" },
      { "Part 4 ON", "パート4 ON" },
      { "Part 5 ON", "パート5 ON" },
      { "Part 6 ON", "パート6 ON" },

      { "Part 1 OFF", "パート1 OFF" },
      { "Part 2 OFF", "パート2 OFF" },
      { "Part 3 OFF", "パート3 OFF" },
      { "Part 4 OFF", "パート4 OFF" },
      { "Part 5 OFF", "パート5 OFF" },
      { "Part 6 OFF", "パート6 OFF" },

      { "Part 1 Edit", "パート1 編集" },
      { "Part 2 Edit", "パート2 編集" },
      { "Part 3 Edit", "パート3 編集" },
      { "Part 4 Edit", "パート4 編集" },
      { "Part 5 Edit", "パート5 編集" },
      { "Part 6 Edit", "パート6 編集" },

      { "---", "---" },

    }};
    static constexpr const def::command::command_param_array_t listitem[] = {
      { def::command::internal_button, 1 }, { def::command::internal_button, 2 }, { def::command::internal_button, 3 }, { def::command::internal_button, 4 }, { def::command::internal_button, 5 },
      { def::command::internal_button, 6 }, { def::command::internal_button, 7 }, { def::command::internal_button, 8 }, { def::command::internal_button, 9 }, { def::command::internal_button,10 },
      { def::command::internal_button,11 }, { def::command::internal_button,12 }, { def::command::internal_button,13 }, { def::command::internal_button,14 }, { def::command::internal_button,15 },

      { def::command::internal_button,16 }, { def::command::internal_button,17 }, { def::command::internal_button,18 }, { def::command::internal_button,19 },
      
      { def::command::internal_button,22, def::command::internal_button,16 },
      { def::command::internal_button,22, def::command::internal_button,17 },
      { def::command::internal_button,22, def::command::internal_button,18 },
      { def::command::internal_button,22, def::command::internal_button,19 },
      
      { def::command::internal_button,20 },
      { def::command::internal_button,21 }, { def::command::internal_button,22 }, { def::command::internal_button,23 }, { def::command::internal_button,24 }, { def::command::internal_button,25 },
      { def::command::internal_button,26 }, { def::command::internal_button,27 }, { def::command::internal_button,28 }, { def::command::internal_button,29 }, { def::command::internal_button,30 },
      { def::command::internal_button,31 }, { def::command::internal_button,32 },

      { def::command::chord_degree, 1 },
      { def::command::chord_semitone, 1, def::command::chord_degree, 2 },
      { def::command::chord_degree, 2 },
      { def::command::chord_semitone, 1, def::command::chord_degree, 3 },
      { def::command::chord_degree, 3 },
      { def::command::chord_degree, 4 },
      { def::command::chord_semitone, 1, def::command::chord_degree, 5 },
      { def::command::chord_degree, 5 },
      { def::command::chord_semitone, 1, def::command::chord_degree, 6 },
      { def::command::chord_degree, 6 },
      { def::command::chord_semitone, 1, def::command::chord_degree, 7 },
      { def::command::chord_degree, 7 },

      { def::command::chord_minor_swap, 1,                                  def::command::chord_degree, 1 },
      { def::command::chord_minor_swap, 1, def::command::chord_semitone, 1, def::command::chord_degree, 2 },
      { def::command::chord_minor_swap, 1,                                  def::command::chord_degree, 2 },
      { def::command::chord_minor_swap, 1, def::command::chord_semitone, 1, def::command::chord_degree, 3 },
      { def::command::chord_minor_swap, 1,                                  def::command::chord_degree, 3 },
      { def::command::chord_minor_swap, 1,                                  def::command::chord_degree, 4 },
      { def::command::chord_minor_swap, 1, def::command::chord_semitone, 1, def::command::chord_degree, 5 },
      { def::command::chord_minor_swap, 1,                                  def::command::chord_degree, 5 },
      { def::command::chord_minor_swap, 1, def::command::chord_semitone, 1, def::command::chord_degree, 6 },
      { def::command::chord_minor_swap, 1,                                  def::command::chord_degree, 6 },
      { def::command::chord_minor_swap, 1, def::command::chord_semitone, 1, def::command::chord_degree, 7 },
      { def::command::chord_minor_swap, 1,                                  def::command::chord_degree, 7 },

      { def::command::chord_modifier, KANTANMusic_Modifier_7    ,                                    def::command::chord_degree, 1 },
      { def::command::chord_modifier, KANTANMusic_Modifier_M7   ,                                    def::command::chord_degree, 1 },
      { def::command::chord_modifier, KANTANMusic_Modifier_7    ,                                    def::command::chord_degree, 2 },
      { def::command::chord_modifier, KANTANMusic_Modifier_7    ,                                    def::command::chord_degree, 3 },
      { def::command::chord_modifier, KANTANMusic_Modifier_7    , def::command::chord_minor_swap, 1, def::command::chord_degree, 3 },
      { def::command::chord_modifier, KANTANMusic_Modifier_7    ,                                    def::command::chord_degree, 4 },
      { def::command::chord_modifier, KANTANMusic_Modifier_M7   ,                                    def::command::chord_degree, 4 },
      { def::command::chord_modifier, KANTANMusic_Modifier_7    ,                                    def::command::chord_degree, 5 },
      { def::command::chord_modifier, KANTANMusic_Modifier_7    ,                                    def::command::chord_degree, 6 },
      { def::command::chord_modifier, KANTANMusic_Modifier_7    ,                                    def::command::chord_degree, 7 },
      { def::command::chord_modifier, KANTANMusic_Modifier_7    , def::command::chord_minor_swap, 1, def::command::chord_degree, 7 },
      { def::command::chord_modifier, KANTANMusic_Modifier_m7_5 ,                                    def::command::chord_degree, 7 },
      { def::command::chord_modifier, KANTANMusic_Modifier_dim  ,                                    def::command::chord_degree, 7 },

      { def::command::chord_modifier  , KANTANMusic_Modifier_dim  },
      { def::command::chord_modifier  , KANTANMusic_Modifier_m7_5 },
      { def::command::chord_modifier  , KANTANMusic_Modifier_sus4 },
      { def::command::chord_modifier  , KANTANMusic_Modifier_6    },
      { def::command::chord_modifier  , KANTANMusic_Modifier_7    },
      { def::command::chord_modifier  , KANTANMusic_Modifier_Add9 },
      { def::command::chord_modifier  , KANTANMusic_Modifier_M7   },
      { def::command::chord_modifier  , KANTANMusic_Modifier_aug  },
      { def::command::chord_modifier  , KANTANMusic_Modifier_7sus4},
      { def::command::chord_modifier  , KANTANMusic_Modifier_dim7 },
      { def::command::chord_minor_swap, 1 },
      { def::command::chord_semitone  , 1 },
      { def::command::chord_semitone  , 2 },

      {                                       def::command::chord_bass_degree, 1 },
      { def::command::chord_bass_semitone, 1, def::command::chord_bass_degree, 2 },
      {                                       def::command::chord_bass_degree, 2 },
      { def::command::chord_bass_semitone, 1, def::command::chord_bass_degree, 3 },
      {                                       def::command::chord_bass_degree, 3 },
      {                                       def::command::chord_bass_degree, 4 },
      { def::command::chord_bass_semitone, 1, def::command::chord_bass_degree, 5 },
      {                                       def::command::chord_bass_degree, 5 },
      { def::command::chord_bass_semitone, 1, def::command::chord_bass_degree, 6 },
      {                                       def::command::chord_bass_degree, 6 },
      { def::command::chord_bass_semitone, 1, def::command::chord_bass_degree, 7 },
      {                                       def::command::chord_bass_degree, 7 },

      {                                  def::command::chord_minor_swap, 1, def::command::chord_modifier, KANTANMusic_Modifier_7    },
      { def::command::chord_semitone, 1, def::command::chord_minor_swap, 1,                                                         },
      { def::command::chord_semitone, 1, def::command::chord_minor_swap, 1, def::command::chord_modifier, KANTANMusic_Modifier_7    },
      { def::command::chord_semitone, 1,                                    def::command::chord_modifier, KANTANMusic_Modifier_dim  },
      { def::command::chord_semitone, 2,                                    def::command::chord_modifier, KANTANMusic_Modifier_dim  },
      { def::command::chord_semitone, 1,                                    def::command::chord_modifier, KANTANMusic_Modifier_m7_5 },
      { def::command::chord_semitone, 2,                                    def::command::chord_modifier, KANTANMusic_Modifier_m7_5 },

      { def::command::part_on, 1 },
      { def::command::part_on, 2 },
      { def::command::part_on, 3 },
      { def::command::part_on, 4 },
      { def::command::part_on, 5 },
      { def::command::part_on, 6 },

      { def::command::part_off, 1 },
      { def::command::part_off, 2 },
      { def::command::part_off, 3 },
      { def::command::part_off, 4 },
      { def::command::part_off, 5 },
      { def::command::part_off, 6 },

      { def::command::part_edit, 1 },
      { def::command::part_edit, 2 },
      { def::command::part_edit, 3 },
      { def::command::part_edit, 4 },
      { def::command::part_edit, 5 },
      { def::command::part_edit, 6 },

      { def::command::none }
    };
  public:
    constexpr mi_ext_assign_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, uint8_t button_index)
    : mi_selector_t { cate, seq, level, title, &name_array }
    , _button_index { button_index } {}
  
    int getValue(void) const override
    {
      auto cmd = system_registry.command_mapping_external.getCommandParamArray(_button_index);
      int index = 0;
      for (int i = 0; ; ++i) {
        auto c = listitem[i];
        if (c == cmd) {
          return getMinValue() + i;
        }
        if (c.array[0].command == def::command::none) { break; }
      }
      return getMinValue();
    }
    bool setValue(int value) const override
    {
      if (mi_selector_t::setValue(value) == false) { return false; }
      value -= getMinValue();
      system_registry.command_mapping_external.setCommandParamArray(_button_index, listitem[value]);
      return true;
    }
  };

struct mi_midi_assign_t : public mi_ext_assign_t {
public:
  constexpr mi_midi_assign_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, uint8_t button_index)
  : mi_ext_assign_t { cate, seq, level, title, button_index }
  {}

  int getValue(void) const override
  {
    auto cmd = system_registry.command_mapping_midi.getCommandParamArray(_button_index);
    int index = 0;
    for (int i = 0; ; ++i) {
      auto c = listitem[i];
      if (c == cmd) {
        return getMinValue() + i;
      }
      if (c.array[0].command == def::command::none) { break; }
    }
    return getMinValue();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.command_mapping_midi.setCommandParamArray(_button_index, listitem[value]);
    return true;
  }
};

struct mi_midi_selector_t : public mi_selector_t {
protected:
  static constexpr const localize_text_array_t name_array = { 4, (const localize_text_t[]){
    { "Off",      "オフ" },
    { "Output",   "出力" },
    { "Input",    "入力" },
    { "In + Out", "入出力" },
  }};

public:
  constexpr mi_midi_selector_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array } {}
};

struct mi_portc_midi_t : public mi_midi_selector_t {
  constexpr mi_portc_midi_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_midi_selector_t { cate, seq, level, title } {}
  int getValue(void) const override
  {
    return getMinValue() + system_registry.midi_port_setting.getPortCMIDI();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.midi_port_setting.setPortCMIDI( static_cast<def::command::ex_midi_mode_t>(value));
    return true;
  }
};

struct mi_ble_midi_t : public mi_midi_selector_t {
  constexpr mi_ble_midi_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_midi_selector_t { cate, seq, level, title } {}
  int getValue(void) const override
  {
    return getMinValue() + system_registry.midi_port_setting.getBLEMIDI();
  }
  bool setValue(int value) const override
  {
    if (mi_selector_t::setValue(value) == false) { return false; }
    value -= getMinValue();
    system_registry.midi_port_setting.setBLEMIDI( static_cast<def::command::ex_midi_mode_t>(value));
    return true;
  }
};

struct mi_otaupdate_t : public mi_normal_t {
  constexpr mi_otaupdate_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title } {}
  menu_item_type_t getType(void) const override { return menu_item_type_t::show_progress; }

  bool setSelectingValue(int value) const override { return false; }
  bool execute(void) const override { return false; }
  bool inputUpDown(int updown) const override { return false; }
  bool inputNumber(uint8_t number) const override { return false; }

  bool enter(void) const override
  {
    // OTAを実施する際にオートプレイは無効にする
    system_registry.runtime_info.setChordAutoplayState(def::play::auto_play_mode_t::auto_play_none);

    system_registry.runtime_info.setWiFiOtaProgress(def::command::wifi_ota_state_t::ota_connecting);
    system_registry.wifi_control.setOperation(def::command::wifi_operation_t::wfop_ota_begin);
    return mi_normal_t::enter();
  }
  bool exit(void) const override
  {
    if (getSelectingValue() <= 127) {
      // OTAの途中でメニューを閉じることはできない
      return true;
    }
    system_registry.wifi_control.setOperation(def::command::wifi_operation_t::wfop_disable);
    return mi_normal_t::exit();
  }

  std::string getString(void) const override {
    char buf[32];
    std::string result;
    auto v = getSelectingValue();
    switch (v) {
    case (uint8_t)def::command::wifi_ota_state_t::ota_connecting:         snprintf(buf, sizeof(buf), "Connecting.");           break;
    case (uint8_t)def::command::wifi_ota_state_t::ota_connection_error:   snprintf(buf, sizeof(buf), "Connection error.");     break;
    case (uint8_t)def::command::wifi_ota_state_t::ota_already_up_to_date: snprintf(buf, sizeof(buf), "Already up to date.");   break;
    case (uint8_t)def::command::wifi_ota_state_t::ota_update_failed:      snprintf(buf, sizeof(buf), "Update failed.");        break;

    case 101: snprintf(buf, sizeof(buf), "Update Done.");
      break;

    default:
      snprintf(buf, sizeof(buf), "Download :% 3d %%", v);
      break;
    }
    return std::string(buf);
  }

  int getSelectingValue(void) const override
  {
    return system_registry.runtime_info.getWiFiOtaProgress();
  }
};

struct mi_wifiap_t : public mi_selector_t {
protected:
  static constexpr const localize_text_array_t name_array = { 2, (const localize_text_t[]){
    { "Use Smartphone", "スマホで設定" },
    { "WPS"           , "WPSで設定"   },
  }};

public:
  constexpr mi_wifiap_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_selector_t { cate, seq, level, title, &name_array } {}

  const char* getValueText(void) const override { return "..."; }

  int getSelectingValue(void) const override
  {
    auto qrtype = def::qrcode_type_t::QRCODE_NONE;

    auto result = mi_selector_t::getSelectingValue();

    if (result == 1) {
      if (system_registry.wifi_control.getOperation() == def::command::wifi_operation_t::wfop_setup_ap) {
        qrtype = system_registry.runtime_info.getWiFiStationCount()
                    ? def::qrcode_type_t::QRCODE_URL_DEVICE
                    : def::qrcode_type_t::QRCODE_AP_SSID;
      }
    }
    if (system_registry.popup_qr.getQRCodeType() != qrtype) {
      system_registry.popup_qr.setQRCodeType(qrtype);
      if (result == 1 && qrtype == def::qrcode_type_t::QRCODE_NONE) {
        exit();
      }
    }
    return result;
  }
  bool execute(void) const override
  {
    if (getSelectingValue() == 1) {
      system_registry.wifi_control.setOperation(def::command::wifi_operation_t::wfop_setup_ap);
    } else {
      system_registry.wifi_control.setOperation(def::command::wifi_operation_t::wfop_setup_wps);
    }
    return false;
  }

  bool exit(void) const override
  {
    system_registry.wifi_control.setOperation(def::command::wifi_operation_t::wfop_disable);
    system_registry.popup_qr.setQRCodeType(def::qrcode_type_t::QRCODE_NONE);
    return mi_normal_t::exit();
  }
};

struct mi_manual_qr_t : public mi_normal_t {
  constexpr mi_manual_qr_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title )
  : mi_normal_t { cate, seq, level, title } {}

  const char* getValueText(void) const override { return "..."; }
  const char* getSelectorText(size_t index) const override { return _title.get(); }

  size_t getSelectorCount(void) const override { return 1; }

  bool execute(void) const override
  {
    system_registry.popup_qr.setQRCodeType(def::qrcode_type_t::QRCODE_URL_MANUAL);
    return false;
  }

  bool exit(void) const override
  {
    system_registry.popup_qr.setQRCodeType(def::qrcode_type_t::QRCODE_NONE);
    return mi_normal_t::exit();
  }
};

struct mi_filelist_t : public mi_normal_t {
  constexpr mi_filelist_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, def::app::data_type_t dir_type )
  : mi_normal_t { cate, seq, level, title }
  , _dir_type { dir_type }
  {}
protected:
  def::app::data_type_t _dir_type;

  const char* getSelectorText(size_t index) const override {
    auto fileinfo = file_manage.getFileInfo(_dir_type, index);
    return fileinfo->filename.c_str();
  }

  size_t getSelectorCount(void) const override { return file_manage.getDirManage(_dir_type)->getCount(); }

  const char* getValueText(void) const override
  {
    return "...";
  }

  int getValue(void) const override
  {
    auto songinfo = system_registry.file_command.getCurrentSongInfo();
    if (songinfo.dir_type == _dir_type) {
      return songinfo.file_index + getMinValue();
    }
    return 0;
  }
};

struct mi_load_file_t : public mi_filelist_t {
  constexpr mi_load_file_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, def::app::data_type_t dir_type )
  : mi_filelist_t { cate, seq, level, title, dir_type }
  {}
protected:
  bool enter(void) const override
  {
    system_registry.backup_song_data.assign(system_registry.song_data);
    system_registry.file_command.setUpdateList(_dir_type);
    M5.delay(64);
    return mi_filelist_t::enter();
  }

  bool exit(void) const override
  {
    system_registry.song_data.assign(system_registry.backup_song_data);
    return mi_filelist_t::exit();
  }

  bool execute(void) const override
  {
    auto songinfo = system_registry.file_command.getCurrentSongInfo();
    songinfo.file_index = _selecting_value - getMinValue();
    songinfo.dir_type = _dir_type;
    system_registry.file_command.setFileLoadRequest(songinfo);
    system_registry.file_command.setCurrentSongInfo(songinfo);
    return mi_filelist_t::execute();
  }
};

struct mi_save_t : public mi_normal_t {
  constexpr mi_save_t( def::menu_category_t cate, uint8_t seq, uint8_t level, const localize_text_t& title, def::app::data_type_t dir_type )
  : mi_normal_t { cate, seq, level, title }
  , _dir_type { dir_type }
  {}
  static constexpr const size_t max_filenames = 4;
  def::app::data_type_t _dir_type;
protected:
  const char* getSelectorText(size_t index) const override {
    return _filenames[index].c_str();
  }

  size_t getSelectorCount(void) const override { return max_filenames; }

  const char* getValueText(void) const override
  {
    return "...";
  }

  bool enter(void) const override
  {
    auto fn = file_manage.getDisplayFileName();
    if (fn.empty()) {
      fn = "new_song";
    }
    _filenames[0] = fn + ".json";
    _filenames[1] = fn + "_.json";
    _filenames[2] = "_" + fn + ".json";

    auto t = time(nullptr);
    auto tm = localtime(&t);
    char buf[64];

    snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d.json",
          tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
          tm->tm_hour, tm->tm_min, tm->tm_sec);
    _filenames[3] = buf;

    _selecting_value = getMinValue();

    return mi_normal_t::enter();
  }

  bool execute(void) const override
  {
    auto mem = file_manage.createMemoryInfo(def::app::max_file_len);
    auto index = _selecting_value - getMinValue();
    mem->filename = _filenames[index];
    mem->dir_type = _dir_type;

    system_registry.unchanged_song_data.assign(system_registry.song_data);
    auto len = system_registry.unchanged_song_data.saveSongJSON(mem->data, def::app::max_file_len);
    mem->size = len;
    if (len == 0 || mem->data[0] != '{') {
      system_registry.popup_notify.setPopup(false, def::notify_type_t::NOTIFY_FILE_SAVE);
      M5_LOGE("mi_save_t: saveSongJSON failed");
      return false;
    }
    def::app::file_command_info_t info;
    info.mem_index = mem->index;
    info.dir_type = _dir_type;
    info.file_index = -1;
    system_registry.file_command.setFileSaveRequest(info);
    return mi_normal_t::execute();
  }
protected:
  static std::string _filenames[max_filenames];
};
std::string mi_save_t::_filenames[max_filenames];


static constexpr menu_item_ptr menu_system[] = {
  (const mi_tree_t          []){{ def::menu_category_t::menu_system,  0,0   , { "Menu"           , "メニュー"    }}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system,  1, 1  , { "File"           , "ファイル"    }}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system,  2,  2 , { "Open"           , "開く"        }}},
  (const mi_load_file_t     []){{ def::menu_category_t::menu_system,  3,   3, { "Preset Songs"   , "プリセットソング" }, def::app::data_type_t::data_song_preset }},
  (const mi_load_file_t     []){{ def::menu_category_t::menu_system,  4,   3, { "Extra Songs (SD)","エクストラソング(SD)" }, def::app::data_type_t::data_song_extra }},
  (const mi_load_file_t     []){{ def::menu_category_t::menu_system,  5,   3, { "User Songs (SD)", "ユーザソング(SD)"}, def::app::data_type_t::data_song_users }},
  (const mi_save_t          []){{ def::menu_category_t::menu_system,  6,  2 , { "Save"           , "上書き保存"    }, def::app::data_type_t::data_song_users }},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system,  7, 1  , { "Slot Setting"   , "スロット設定"  }}},
  (const mi_slot_playmode_t []){{ def::menu_category_t::menu_system,  8,  2 , { "Play Mode"      , "演奏モード"    }}},
  (const mi_slot_key_t      []){{ def::menu_category_t::menu_system,  9,  2 , { "Key Modulation" , "キー転調"      }}},
  (const mi_slot_clipboard_t[]){{ def::menu_category_t::menu_system, 10,  2 , { "Clipboard"      , "クリップボード" }}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system, 11, 1  , { "Tempo & Groove" , "テンポ＆グルーヴ設定"  }}},
  (const mi_song_tempo_t    []){{ def::menu_category_t::menu_system, 12,  2 , { "BPM"            , "テンポ(BPM)"   }}},
  (const mi_song_swing_t    []){{ def::menu_category_t::menu_system, 13,  2 , { "Swing"          , "スウィング"    }}},
// TODO:これ追加 Auto / Manual ( 初期値 Auto )
//(const mi_song_swing_t    []){{ def::menu_category_t::menu_system, 14,  2  , { "Offbeat Control", "裏拍演奏"     }}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system, 14, 1   , { "System"         , "システム"     }}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system, 15,  2  , { "WiFi"           , "WiFi通信"    }}},
  (const mi_usewifi_t       []){{ def::menu_category_t::menu_system, 16,   3 , { "Connection"     , "接続"        }}},
  (const mi_otaupdate_t     []){{ def::menu_category_t::menu_system, 17,   3 , { "Firm Update"    , "ファーム更新" }}},
  (const mi_wifiap_t        []){{ def::menu_category_t::menu_system, 18,   3 , { "WiFi Setup"     , "WiFi設定"     }}},

  (const mi_tree_t          []){{ def::menu_category_t::menu_system, 19,  2   , { "Control Assignment", "操作割り当て"   }}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system, 20,   3  , { "Play Button"    , "プレイボタン"   }}},
  (const mi_modifier_t      []){{ def::menu_category_t::menu_system, 21,    4 , { "Button 4"       , "ボタン 4"      },  4 - 1}},
  (const mi_modifier_t      []){{ def::menu_category_t::menu_system, 22,    4 , { "Button 5"       , "ボタン 5"      },  5 - 1}},
  (const mi_modifier_t      []){{ def::menu_category_t::menu_system, 23,    4 , { "Button 9"       , "ボタン 9"      },  9 - 1}},
  (const mi_modifier_t      []){{ def::menu_category_t::menu_system, 24,    4 , { "Button 10"      , "ボタン 10"     }, 10 - 1}},
  (const mi_modifier_t      []){{ def::menu_category_t::menu_system, 25,    4 , { "Button 12"      , "ボタン 12"     }, 12 - 1}},
  (const mi_modifier_t      []){{ def::menu_category_t::menu_system, 26,    4 , { "Button 13"      , "ボタン 13"     }, 13 - 1}},
  (const mi_modifier_t      []){{ def::menu_category_t::menu_system, 27,    4 , { "Button 14"      , "ボタン 14"     }, 14 - 1}},
  (const mi_modifier_t      []){{ def::menu_category_t::menu_system, 28,    4 , { "Button 15"      , "ボタン 15"     }, 15 - 1}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system, 29,   3  , { "Ext Box"        , "拡張ボックス"  }}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 30,    4 , { "Ext 1"          , "拡張ボタン 1"        },  1 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 31,    4 , { "Ext 2"          , "拡張ボタン 2"        },  2 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 32,    4 , { "Ext 3"          , "拡張ボタン 3"        },  3 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 33,    4 , { "Ext 4"          , "拡張ボタン 4"        },  4 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 34,    4 , { "Ext 5"          , "拡張ボタン 5"        },  5 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 35,    4 , { "Ext 6"          , "拡張ボタン 6"        },  6 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 36,    4 , { "Ext 7"          , "拡張ボタン 7"        },  7 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 37,    4 , { "Ext 8"          , "拡張ボタン 8"        },  8 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 38,    4 , { "Ext 9"          , "拡張ボタン 9"        },  9 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 39,    4 , { "Ext 10"         , "拡張ボタン 10"       },  10 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 40,    4 , { "Ext 11"         , "拡張ボタン 11"       },  11 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 41,    4 , { "Ext 12"         , "拡張ボタン 12"       },  12 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 42,    4 , { "Ext 13"         , "拡張ボタン 13"       },  13 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 43,    4 , { "Ext 14"         , "拡張ボタン 14"       },  14 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 44,    4 , { "Ext 15"         , "拡張ボタン 15"       },  15 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 45,    4 , { "Ext 16"         , "拡張ボタン 16"       },  16 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 46,    4 , { "Ext 17"         , "拡張ボタン 17"       },  17 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 47,    4 , { "Ext 18"         , "拡張ボタン 18"       },  18 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 48,    4 , { "Ext 19"         , "拡張ボタン 19"       },  19 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 49,    4 , { "Ext 20"         , "拡張ボタン 20"       },  20 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 50,    4 , { "Ext 21"         , "拡張ボタン 21"       },  21 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 51,    4 , { "Ext 22"         , "拡張ボタン 22"       },  22 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 52,    4 , { "Ext 23"         , "拡張ボタン 23"       },  23 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 53,    4 , { "Ext 24"         , "拡張ボタン 24"       },  24 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 54,    4 , { "Ext 25"         , "拡張ボタン 25"       },  25 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 55,    4 , { "Ext 26"         , "拡張ボタン 26"       },  26 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 56,    4 , { "Ext 27"         , "拡張ボタン 27"       },  27 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 57,    4 , { "Ext 28"         , "拡張ボタン 28"       },  28 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 58,    4 , { "Ext 29"         , "拡張ボタン 29"       },  29 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 59,    4 , { "Ext 30"         , "拡張ボタン 30"       },  30 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 60,    4 , { "Ext 31"         , "拡張ボタン 31"       },  31 - 1}},
  (const mi_ext_assign_t    []){{ def::menu_category_t::menu_system, 61,    4 , { "Ext 32"         , "拡張ボタン 32"       },  32 - 1}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system, 62,   3  , { "MIDI Note"      , "MIDI Note"    }}},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 63,    4 , { "C -1 (0)"   , "C -1 (0)"        },   0 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 64,    4 , { "C#-1 (1)"   , "C#-1 (1)"        },   1 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 65,    4 , { "D -1 (2)"   , "D -1 (2)"        },   2 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 66,    4 , { "D#-1 (3)"   , "D#-1 (3)"        },   3 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 67,    4 , { "E -1 (4)"   , "E -1 (4)"        },   4 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 68,    4 , { "F -1 (5)"   , "F -1 (5)"        },   5 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 69,    4 , { "F#-1 (6)"   , "F#-1 (6)"        },   6 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 70,    4 , { "G -1 (7)"   , "G -1 (7)"        },   7 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 71,    4 , { "G#-1 (8)"   , "G#-1 (8)"        },   8 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 72,    4 , { "A -1 (9)"   , "A -1 (9)"        },   9 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 73,    4 , { "A#-1 (10)"  , "A#-1 (10)"       },  10 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 74,    4 , { "B -1 (11)"  , "B -1 (11)"       },  11 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 75,    4 , { "C  0 (12)"  , "C  0 (12)"       },  12 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 76,    4 , { "C# 0 (13)"  , "C# 0 (13)"       },  13 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 77,    4 , { "D  0 (14)"  , "D  0 (14)"       },  14 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 78,    4 , { "D# 0 (15)"  , "D# 0 (15)"       },  15 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 79,    4 , { "E  0 (16)"  , "E  0 (16)"       },  16 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 80,    4 , { "F  0 (17)"  , "F  0 (17)"       },  17 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 81,    4 , { "F# 0 (18)"  , "F# 0 (18)"       },  18 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 82,    4 , { "G  0 (19)"  , "G  0 (19)"       },  19 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 83,    4 , { "G# 0 (20)"  , "G# 0 (20)"       },  20 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 84,    4 , { "A  0 (21)"  , "A  0 (21)"       },  21 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 85,    4 , { "A# 0 (22)"  , "A# 0 (22)"       },  22 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 86,    4 , { "B  0 (23)"  , "B  0 (23)"       },  23 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 87,    4 , { "C  1 (24)"  , "C  1 (24)"       },  24 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 88,    4 , { "C# 1 (25)"  , "C# 1 (25)"       },  25 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 89,    4 , { "D  1 (26)"  , "D  1 (26)"       },  26 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 90,    4 , { "D# 1 (27)"  , "D# 1 (27)"       },  27 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 91,    4 , { "E  1 (28)"  , "E  1 (28)"       },  28 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 92,    4 , { "F  1 (29)"  , "F  1 (29)"       },  29 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 93,    4 , { "F# 1 (30)"  , "F# 1 (30)"       },  30 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 94,    4 , { "G  1 (31)"  , "G  1 (31)"       },  31 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 95,    4 , { "G# 1 (32)"  , "G# 1 (32)"       },  32 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 96,    4 , { "A  1 (33)"  , "A  1 (33)"       },  33 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 97,    4 , { "A# 1 (34)"  , "A# 1 (34)"       },  34 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 98,    4 , { "B  1 (35)"  , "B  1 (35)"       },  35 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system, 99,    4 , { "C  2 (36)"  , "C  2 (36)"       },  36 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,100,    4 , { "C# 2 (37)"  , "C# 2 (37)"       },  37 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,101,    4 , { "D  2 (38)"  , "D  2 (38)"       },  38 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,102,    4 , { "D# 2 (39)"  , "D# 2 (39)"       },  39 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,103,    4 , { "E  2 (40)"  , "E  2 (40)"       },  40 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,104,    4 , { "F  2 (41)"  , "F  2 (41)"       },  41 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,105,    4 , { "F# 2 (42)"  , "F# 2 (42)"       },  42 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,106,    4 , { "G  2 (43)"  , "G  2 (43)"       },  43 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,107,    4 , { "G# 2 (44)"  , "G# 2 (44)"       },  44 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,108,    4 , { "A  2 (45)"  , "A  2 (45)"       },  45 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,109,    4 , { "A# 2 (46)"  , "A# 2 (46)"       },  46 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,110,    4 , { "B  2 (47)"  , "B  2 (47)"       },  47 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,111,    4 , { "C  3 (48)"  , "C  3 (48)"       },  48 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,112,    4 , { "C# 3 (49)"  , "C# 3 (49)"       },  49 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,113,    4 , { "D  3 (50)"  , "D  3 (50)"       },  50 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,114,    4 , { "D# 3 (51)"  , "D# 3 (51)"       },  51 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,115,    4 , { "E  3 (52)"  , "E  3 (52)"       },  52 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,116,    4 , { "F  3 (53)"  , "F  3 (53)"       },  53 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,117,    4 , { "F# 3 (54)"  , "F# 3 (54)"       },  54 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,118,    4 , { "G  3 (55)"  , "G  3 (55)"       },  55 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,119,    4 , { "G# 3 (56)"  , "G# 3 (56)"       },  56 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,120,    4 , { "A  3 (57)"  , "A  3 (57)"       },  57 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,121,    4 , { "A# 3 (58)"  , "A# 3 (58)"       },  58 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,122,    4 , { "B  3 (59)"  , "B  3 (59)"       },  59 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,123,    4 , { "C  4 (60)"  , "C  4 (60)"       },  60 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,124,    4 , { "C# 4 (61)"  , "C# 4 (61)"       },  61 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,125,    4 , { "D  4 (62)"  , "D  4 (62)"       },  62 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,126,    4 , { "D# 4 (63)"  , "D# 4 (63)"       },  63 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,127,    4 , { "E  4 (64)"  , "E  4 (64)"       },  64 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,128,    4 , { "F  4 (65)"  , "F  4 (65)"       },  65 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,129,    4 , { "F# 4 (66)"  , "F# 4 (66)"       },  66 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,130,    4 , { "G  4 (67)"  , "G  4 (67)"       },  67 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,131,    4 , { "G# 4 (68)"  , "G# 4 (68)"       },  68 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,132,    4 , { "A  4 (69)"  , "A  4 (69)"       },  69 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,133,    4 , { "A# 4 (70)"  , "A# 4 (70)"       },  70 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,134,    4 , { "B  4 (71)"  , "B  4 (71)"       },  71 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,135,    4 , { "C  5 (72)"  , "C  5 (72)"       },  72 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,136,    4 , { "C# 5 (73)"  , "C# 5 (73)"       },  73 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,137,    4 , { "D  5 (74)"  , "D  5 (74)"       },  74 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,138,    4 , { "D# 5 (75)"  , "D# 5 (75)"       },  75 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,139,    4 , { "E  5 (76)"  , "E  5 (76)"       },  76 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,140,    4 , { "F  5 (77)"  , "F  5 (77)"       },  77 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,141,    4 , { "F# 5 (78)"  , "F# 5 (78)"       },  78 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,142,    4 , { "G  5 (79)"  , "G  5 (79)"       },  79 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,143,    4 , { "G# 5 (80)"  , "G# 5 (80)"       },  80 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,144,    4 , { "A  5 (81)"  , "A  5 (81)"       },  81 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,145,    4 , { "A# 5 (82)"  , "A# 5 (82)"       },  82 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,146,    4 , { "B  5 (83)"  , "B  5 (83)"       },  83 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,147,    4 , { "C  6 (84)"  , "C  6 (84)"       },  84 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,148,    4 , { "C# 6 (85)"  , "C# 6 (85)"       },  85 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,149,    4 , { "D  6 (86)"  , "D  6 (86)"       },  86 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,150,    4 , { "D# 6 (87)"  , "D# 6 (87)"       },  87 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,151,    4 , { "E  6 (88)"  , "E  6 (88)"       },  88 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,152,    4 , { "F  6 (89)"  , "F  6 (89)"       },  89 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,153,    4 , { "F# 6 (90)"  , "F# 6 (90)"       },  90 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,154,    4 , { "G  6 (91)"  , "G  6 (91)"       },  91 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,155,    4 , { "G# 6 (92)"  , "G# 6 (92)"       },  92 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,156,    4 , { "A  6 (93)"  , "A  6 (93)"       },  93 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,157,    4 , { "A# 6 (94)"  , "A# 6 (94)"       },  94 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,158,    4 , { "B  6 (95)"  , "B  6 (95)"       },  95 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,159,    4 , { "C  7 (96)"  , "C  7 (96)"       },  96 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,160,    4 , { "C# 7 (97)"  , "C# 7 (97)"       },  97 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,161,    4 , { "D  7 (98)"  , "D  7 (98)"       },  98 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,162,    4 , { "D# 7 (99)"  , "D# 7 (99)"       },  99 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,163,    4 , { "E  7 (100)" , "E  7 (100)"      }, 100 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,164,    4 , { "F  7 (101)" , "F  7 (101)"      }, 101 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,165,    4 , { "F# 7 (102)" , "F# 7 (102)"      }, 102 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,166,    4 , { "G  7 (103)" , "G  7 (103)"      }, 103 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,167,    4 , { "G# 7 (104)" , "G# 7 (104)"      }, 104 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,168,    4 , { "A  7 (105)" , "A  7 (105)"      }, 105 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,169,    4 , { "A# 7 (106)" , "A# 7 (106)"      }, 106 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,170,    4 , { "B  7 (107)" , "B  7 (107)"      }, 107 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,171,    4 , { "C  8 (108)" , "C  8 (108)"      }, 108 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,172,    4 , { "C# 8 (109)" , "C# 8 (109)"      }, 109 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,173,    4 , { "D  8 (110)" , "D  8 (110)"      }, 110 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,174,    4 , { "D# 8 (111)" , "D# 8 (111)"      }, 111 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,175,    4 , { "E  8 (112)" , "E  8 (112)"      }, 112 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,176,    4 , { "F  8 (113)" , "F  8 (113)"      }, 113 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,177,    4 , { "F# 8 (114)" , "F# 8 (114)"      }, 114 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,178,    4 , { "G  8 (115)" , "G  8 (115)"      }, 115 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,179,    4 , { "G# 8 (116)" , "G# 8 (116)"      }, 116 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,180,    4 , { "A  8 (117)" , "A  8 (117)"      }, 117 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,181,    4 , { "A# 8 (118)" , "A# 8 (118)"      }, 118 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,182,    4 , { "B  8 (119)" , "B  8 (119)"      }, 119 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,183,    4 , { "C  9 (120)" , "C  9 (120)"      }, 120 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,184,    4 , { "C# 9 (121)" , "C# 9 (121)"      }, 121 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,185,    4 , { "D  9 (122)" , "D  9 (122)"      }, 122 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,186,    4 , { "D# 9 (123)" , "D# 9 (123)"      }, 123 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,187,    4 , { "E  9 (124)" , "E  9 (124)"      }, 124 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,188,    4 , { "F  9 (125)" , "F  9 (125)"      }, 125 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,189,    4 , { "F# 9 (126)" , "F# 9 (126)"      }, 126 }},
  (const mi_midi_assign_t   []){{ def::menu_category_t::menu_system,190,    4 , { "G  9 (127)" , "G  9 (127)"      }, 127 }},

  (const mi_tree_t          []){{ def::menu_category_t::menu_system,191,  2   , { "External Device", "外部デバイス" }}},
  (const mi_portc_midi_t    []){{ def::menu_category_t::menu_system,192,   3  , { "PortC MIDI"     , "ポートC MIDI" }}},
  (const mi_ble_midi_t      []){{ def::menu_category_t::menu_system,193,   3  , { "BLE MIDI"       , "BLE MIDI"     }}},
// TODO:これ追加  OFF,80,81-89,90 (初期値80)
//(const mi_ble_midi_t      []){{ def::menu_category_t::menu_system, 31,   3 , { "#CC Assignment" , "#CC割当"     }}},
  (const mi_imu_velocity_t  []){{ def::menu_category_t::menu_system,194,  2  , { "IMU Velocity"   , "IMUベロシティ"}}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system,195,  2  , { "Display"        , "表示"        }}},
  (const mi_lcd_backlight_t []){{ def::menu_category_t::menu_system,196,   3 , { "Backlight"      , "画面の輝度"  }}},
  (const mi_led_brightness_t[]){{ def::menu_category_t::menu_system,197,   3 , { "LED Brightness" , "LEDの輝度"   }}},
  (const mi_detail_view_t   []){{ def::menu_category_t::menu_system,198,   3 , { "Detail View"    , "詳細表示"    }}},
  (const mi_wave_view_t     []){{ def::menu_category_t::menu_system,199,   3 , { "Wave View"      , "波形表示"    }}},
  (const mi_language_t      []){{ def::menu_category_t::menu_system,200,  2  , { "Language"       , "言語"        }}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_system,201,  2  , { "Volume"         , "音量"        }}},
  (const mi_vol_midi_t      []){{ def::menu_category_t::menu_system,202,   3 , { "MIDI Mastervol" , "MIDIマスター音量"}}},
  (const mi_vol_adcmic_t    []){{ def::menu_category_t::menu_system,203,   3 , { "ADC MicAmp"     , "ADCマイクアンプ" }}},
  (const mi_all_reset_t     []){{ def::menu_category_t::menu_system,204,  2  , { "Reset All Settings", "全設定リセット"    }}},
  (const mi_manual_qr_t     []){{ def::menu_category_t::menu_system,205, 1   , { "Manual QR"      , "説明書QR"     }}},
  nullptr, // end of menu
};
// const size_t menu_system_size = sizeof(menu_system) / sizeof(menu_system[0]) - 1;

static constexpr menu_item_ptr menu_part[] = {
  (const mi_tree_t          []){{ def::menu_category_t::menu_part,  0,0  , { "Part"       , "パート"           }}},
  (const mi_program_t       []){{ def::menu_category_t::menu_part,  1, 1 , { "Tone"       , "音色"             }}},
  (const mi_octave_t        []){{ def::menu_category_t::menu_part,  2, 1 , { "Octave"     , "オクターブ"       }}},
  (const mi_voicing_t       []){{ def::menu_category_t::menu_part,  3, 1 , { "Voicing"    , "ボイシング"       }}},
  (const mi_velocity_t      []){{ def::menu_category_t::menu_part,  4, 1 , { "Velocity"   , "ベロシティ値"     }}},
  (const mi_partvolume_t    []){{ def::menu_category_t::menu_part,  5, 1 , { "Part Volume", "パート音量"       }}},
  (const mi_loop_length_t   []){{ def::menu_category_t::menu_part,  6, 1 , { "Loop Length", "ループ長"         }}},
  (const mi_anchor_step_t   []){{ def::menu_category_t::menu_part,  7, 1 , { "Anchor Step", "アンカーステップ" }}},
  (const mi_stroke_speed_t  []){{ def::menu_category_t::menu_part,  8, 1 , { "Stroke Speed", "ストローク速度"  }}},
  (const mi_tree_t          []){{ def::menu_category_t::menu_part,  9, 1 , { "DrumNote"   , "ドラムノート"     }}},
  (const mi_drum_note_t     []){{ def::menu_category_t::menu_part, 10,  2, { "Pitch1"     , "ピッチ1"          }, 0}},
  (const mi_drum_note_t     []){{ def::menu_category_t::menu_part, 11,  2, { "Pitch2"     , "ピッチ2"          }, 1}},
  (const mi_drum_note_t     []){{ def::menu_category_t::menu_part, 12,  2, { "Pitch3"     , "ピッチ3"          }, 2}},
  (const mi_drum_note_t     []){{ def::menu_category_t::menu_part, 13,  2, { "Pitch4"     , "ピッチ4"          }, 3}},
  (const mi_drum_note_t     []){{ def::menu_category_t::menu_part, 14,  2, { "Pitch5"     , "ピッチ5"          }, 4}},
  (const mi_drum_note_t     []){{ def::menu_category_t::menu_part, 15,  2, { "Pitch6"     , "ピッチ6"          }, 5}},
  (const mi_drum_note_t     []){{ def::menu_category_t::menu_part, 16,  2, { "Pitch7"     , "ピッチ7"          }, 6}},
  (const mi_part_clipboard_t[]){{ def::menu_category_t::menu_part, 17, 1 , { "Clipboard"      , "クリップボード" }}},
  (const mi_clear_notes_t   []){{ def::menu_category_t::menu_part, 18, 1 , { "Clear All Notes", "ノートをクリア"}}},
  nullptr, // end of menu
};
// const size_t menu_part_size = sizeof(menu_part) / sizeof(menu_part[0]) - 1;

static constexpr menu_item_ptr menu_file[] = {
  (const mi_tree_t        []){{ def::menu_category_t::menu_file,  0,0  , { "File"           , "ファイル"       }}},
  (const mi_load_file_t   []){{ def::menu_category_t::menu_file,  1, 1 , { "Load"           , "読込"           }, def::app::data_type_t::data_song_users }},
  (const mi_load_file_t   []){{ def::menu_category_t::menu_file,  2, 1 , { "Load Preset"    , "プリセット読込"  }, def::app::data_type_t::data_song_extra }},
  (const mi_save_t        []){{ def::menu_category_t::menu_file,  3, 1 , { "Save"           , "保存"           }, def::app::data_type_t::data_song_users }},
  // (const mi_save_new_t    []){{ def::menu_category_t::menu_file,  5,  2, { "New File"       , "新規作成"        }, def::app::data_type_t::data_song_users }},
  // (const mi_filelist_t    []){{ def::menu_category_t::menu_file,  4, 1 , { "Save(Overwrite)", "上書き保存"      }, def::app::data_type_t::data_song_users}},
  nullptr, // end of menu
};

void menu_control_t::openMenu(def::menu_category_t category)
{
  system_registry.menu_status.reset();
  system_registry.menu_status.setCurrentLevel(0);
  system_registry.menu_status.setMenuCategory( category );
  system_registry.menu_status.setSelectIndex(0, 1);

  _menu_array = getMenuArray(category);
  _category = category;

  // system_registry.runtime_info.setPlayMode( def::playmode::playmode_t::menu_mode );
  system_registry.runtime_info.setMenuVisible( true );
}

bool menu_control_t::enter(void)
{
  auto current_level = system_registry.menu_status.getCurrentLevel();
  auto select_index = system_registry.menu_status.getSelectIndex(current_level);
  auto current_seq = system_registry.menu_status.getCurrentSequence();
  if (current_seq == select_index) {
    return _menu_array[select_index]->execute();
  }
  return _menu_array[select_index]->enter();
}

bool menu_control_t::exit(void)
{
  auto current_index = system_registry.menu_status.getCurrentSequence();
  return _menu_array[current_index]->exit();
}

bool menu_control_t::inputNumber(uint8_t number)
{
  auto current_index = system_registry.menu_status.getCurrentSequence();
  return _menu_array[current_index]->inputNumber(number);
}

bool menu_control_t::inputUpDown(int updown)
{
  auto current_index = system_registry.menu_status.getCurrentSequence();

  return _menu_array[current_index]->inputUpDown(updown);
}

// size_t menu_control_t::getChildrenSequenceList(uint8_t* index_list, size_t size, uint8_t parent_index)
// {
//   return getSubMenuIndexList(index_list, _menu_array, size, parent_index);
// }

int menu_control_t::getChildrenSequenceList(std::vector<uint16_t>* index_list, uint8_t parent_index)
{
  return getSubMenuIndexList(index_list, _menu_array, parent_index);
}

#if defined ( M5UNIFIED_PC_BUILD )
// メニューの定義部に間違いがないか確認する関数
// PCビルド時のみ有効
static bool menu_seq_check(const menu_item_ptr_array &menu)
{
  auto cat = menu[0]->getCategory();
  for (size_t i = 0; menu[i] != nullptr; ++i) {
    if (menu[i]->getCategory() != cat) {
      return false;
    }
    if (menu[i]->getSequence() != i) {
      return false;
    }
  }
  return true;
}
#endif

static menu_item_ptr_array getMenuArray(def::menu_category_t category)
{
#if defined ( M5UNIFIED_PC_BUILD )
  assert(menu_seq_check(menu_system) && "menu_system definition error");
  assert(menu_seq_check(menu_part) && "menu_part definition error");
  assert(menu_seq_check(menu_file) && "menu_file definition error");
#endif

  switch (category) {
  default:
  case def::menu_category_t::menu_system:
    return menu_system;
  case def::menu_category_t::menu_part:
    return menu_part;
  case def::menu_category_t::menu_file:
    return menu_file;
  }
  return nullptr;
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns
