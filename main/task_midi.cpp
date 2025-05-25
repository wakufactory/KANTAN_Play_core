// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#include <M5Unified.h>

#include "task_midi.hpp"

#include "common_define.hpp"
#include "system_registry.hpp"
// #include "driver_midi.hpp"

#include "midi/midi_transport_uart.hpp"
#include "midi/midi_transport_ble.hpp"

#if __has_include(<freertos/freertos.h>)
 #include <freertos/FreeRTOS.h>
 #include <freertos/task.h>
#endif

namespace kanplay_ns {
//-------------------------------------------------------------------------
class subtask_midi_t {
private:
  midi_driver::MIDIDriver _midi;
  system_registry_t::reg_task_status_t::bitindex_t _task_status_index;

public:
  subtask_midi_t(midi_driver::MIDI_Transport* transport, system_registry_t::reg_task_status_t::bitindex_t task_status_index)
  : _midi { transport }
  , _task_status_index { task_status_index }
  {
  }

  static void task_func(subtask_midi_t* me)
  {
    registry_t::history_code_t history_code_midi_out = 0;
    uint8_t prev_midi_volume = 0;
    bool prev_tx_enable = false;
    bool prev_rx_enable = false;
    uint8_t channel_volume[def::midi::channel_max];
    uint8_t program_number[def::midi::channel_max];
    auto midi = &(me->_midi);

    for (;;) {
      if (!prev_rx_enable) {
        system_registry.task_status.setSuspend(me->_task_status_index);
      }
  #if defined (M5UNIFIED_PC_BUILD)
      M5.delay(1);
  #else
      ulTaskNotifyTake(pdTRUE, prev_rx_enable ? 1 : 2048);
  #endif
      bool tx_enable = midi->getEnableTx();
      bool rx_enable = midi->getEnableRx();
      if (tx_enable || rx_enable) {
        system_registry.task_status.setWorking(me->_task_status_index);
      }
      if (rx_enable) {
        prev_rx_enable = rx_enable;
        midi->receive();
        midi_driver::MIDI_Message message;
        while (midi->receiveMessage(&message)) {
// printf("status:%02x  len:%d  data:%02x %02x", message.status, message.data.size(), message.data[0], message.data[1]);
          uint8_t channel = message.channel;
          if ((channel == 0) && ((message.type & ~1) == 0x08)) {
            uint8_t note = message.data[0];
            auto command_param_array = system_registry.command_mapping_midinote.getCommandParamArray(note);
            if (!command_param_array.empty()) {
              uint8_t velocity = (message.type == 0x09) // NoteOn
                               ? message.data[1]
                               : 0;
              if (velocity) {
                system_registry.operator_command.addQueue( { def::command::set_velocity, velocity } );
              }
              for (auto command_param : command_param_array.array) {
                uint8_t command = command_param.getCommand();
                if (command == 0) { continue; }
                system_registry.operator_command.addQueue(command_param, velocity ? true : false);
              }
            }
          }
        }
      }

      if (prev_tx_enable != tx_enable) {
        prev_tx_enable = tx_enable;
        if (tx_enable) {
          prev_midi_volume = 0;
          for (int i = 0; i < 16; ++i) {
            // チャンネルボリュームおよびプログラムチェンジを設定
            uint8_t vol = channel_volume[i];
            midi->sendControlChange(def::midi::channel_1 + i, 7, vol);
            uint8_t prg = program_number[i];
            midi->sendProgramChange(def::midi::channel_1 + i, prg);
          }
        }
      }
      if (tx_enable) {
        auto midi_volume = system_registry.user_setting.getMIDIMasterVolume();
        if (prev_midi_volume != midi_volume) {
          prev_midi_volume = midi_volume;
          // マスターボリューム設定
          midi->sendControlChange(def::midi::channel_1, 99, 55);
          midi->sendControlChange(def::midi::channel_1, 98,  7);
          midi->sendControlChange(def::midi::channel_1,  6, midi_volume);
        }
        const registry_t::history_t* history;
        while (nullptr != (history = system_registry.midi_out_control.getHistory(history_code_midi_out))) {
          if (system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_NOTE_CH1 <= history->index && history->index < system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_NOTE_END)
          {
            int index = history->index - system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_NOTE_CH1;
            auto channel = index >> 7;
            auto note = index & 0x7F;
            auto velocity = history->value;
            velocity = (velocity > 0x80) ? velocity & 0x7F : 0;

            midi->sendNoteOn(channel, note, velocity);
          }
          else if (system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_PROGRAM_CH1 <= history->index && history->index < system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_PROGRAM_END) {
            int channel = history->index - system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_PROGRAM_CH1;
            auto value = history->value & 0x7F;
            if (program_number[channel] != value) {
              program_number[channel] = value;
              midi->sendProgramChange(channel, value);
            }
          }
          else if (system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_VOLUME_CH1 <= history->index && history->index < system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_VOLUME_END) {
            int channel = history->index - system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_VOLUME_CH1;
            auto value = history->value & 0x7F;
            if (channel_volume[channel] != value) {
              midi->sendControlChange(channel, 7, value);
              channel_volume[channel] = value;
            }
          }
          else if (system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_CHANGE_START <= history->index && history->index < system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_CHANGE_END) {
            int cc = history->index - system_registry_t::reg_midi_out_control_t::MIDI_CONTROL_CHANGE_START;
            auto value = history->value & 0x7F;
  M5_LOGV("cc: %d, value: %d", cc, value);
            for (int channel = def::midi::channel_1; channel < def::midi::channel_max; ++channel) {
              midi->sendControlChange(channel, cc, value);
            }
          }
        }
        midi->sendFlush();
      } else {
        history_code_midi_out = system_registry.midi_out_control.getHistoryCode();
      }
    }
  }
};

#if defined (M5UNIFIED_PC_BUILD)
//  static windows_midi_transport_t windows_midi_transport; // かんぷれ内部MIDI
//  static subtask_midi_t subtask_array[] = {
//    { &windows_midi_transport, system_registry_t::reg_task_status_t::bitindex_t::TASK_MIDI_INTERNAL },
//  };
static subtask_midi_t subtask_array[] = {};

#else

static midi_driver::MIDI_Transport_UART in_uart_midi_transport; // かんぷれ内部MIDI
static midi_driver::MIDI_Transport_UART portc_midi_transport; // PortC外部MIDI

#ifdef MIDI_TRANSPORT_BLE_HPP
static midi_driver::MIDI_Transport_BLE ble_midi_transport; // BLE-MIDI
#endif

// static uart_midi_transport_t in_uart_midi_transport; // かんぷれ内部MIDI
// static uart_midi_transport_t ex_uart_midi_transport; // PortC外部MIDI
// static ble_midi_transport_t ble_midi_transport; // BLE MIDI
// static usb_midi_transport_t usb_midi_transport; // USB MIDI
static subtask_midi_t subtask_array[] = {
  { &in_uart_midi_transport, system_registry_t::reg_task_status_t::bitindex_t::TASK_MIDI_INTERNAL },
  { &portc_midi_transport  , system_registry_t::reg_task_status_t::bitindex_t::TASK_MIDI_EXTERNAL },
#ifdef MIDI_TRANSPORT_BLE_HPP
  { &ble_midi_transport    , system_registry_t::reg_task_status_t::bitindex_t::TASK_MIDI_BLE },
#endif
// {&ble_midi_transport, system_registry_t::reg_task_status_t::bitindex_t::TASK_MIDI_BLE }, 
// {&usb_midi_transport, system_registry_t::reg_task_status_t::bitindex_t::TASK_MIDI_USB }, 
};
#endif
static constexpr const size_t max_subtask = sizeof(subtask_array)/sizeof(subtask_array[0]);

#if defined (M5UNIFIED_PC_BUILD)
 SDL_Thread* subtask_handle[max_subtask];
#else
 TaskHandle_t subtask_handle[max_subtask];
#endif

void task_midi_t::start(void)
{

#if defined (M5UNIFIED_PC_BUILD)
  // windows_midi_transport_t::config_t config;

  // config.deviceID = 0;
  // windows_midi_transport.init(config);
  // windows_midi_transport.changeEnable(true, false);

  // auto thread = SDL_CreateThread((SDL_ThreadFunction)task_func, "midi", this);
  for (int i = 0; i < max_subtask; ++i) {
    subtask_handle[i] = SDL_CreateThread((SDL_ThreadFunction)subtask_midi_t::task_func, "midi_subtask", &subtask_array[i]);
  }
#else
  {
    midi_driver::MIDI_Transport_UART::config_t config;

    // 内部SAM音源用MIDI
    config.uart_port_num = 1; // UART_NUM_1;
    config.pin_tx = def::hw::pin::midi_tx;
    config.pin_rx = GPIO_NUM_NC;
    in_uart_midi_transport.setConfig(config);
    in_uart_midi_transport.begin();
    in_uart_midi_transport.setEnable(true, false);

    // 外部PortC用MIDI
    config.uart_port_num = 2; // UART_NUM_2
    config.pin_tx = M5.getPin(m5::pin_name_t::port_c_txd);
    config.pin_rx = M5.getPin(m5::pin_name_t::port_c_rxd);
    portc_midi_transport.setConfig(config);
    portc_midi_transport.begin();
    // オン・オフはsystem_registryで設定する
  }
#ifdef MIDI_TRANSPORT_BLE_HPP
  {
    midi_driver::MIDI_Transport_BLE::config_t config;
    ble_midi_transport.setConfig(config);
    ble_midi_transport.begin();
    // オン・オフはsystem_registryで設定する
  }
#endif

  TaskHandle_t handle = nullptr;
  xTaskCreatePinnedToCore((TaskFunction_t)task_func, "midi", 1024*3, this, def::system::task_priority_midi, &handle, def::system::task_cpu_midi);
  system_registry.midi_out_control.setNotifyTaskHandle(handle);
  system_registry.midi_port_setting.setNotifyTaskHandle(handle);

  for (int i = 0; i < max_subtask; ++i) {
    xTaskCreatePinnedToCore((TaskFunction_t)subtask_midi_t::task_func, "midi_subtask", 1024*3, &subtask_array[i], def::system::task_priority_midi_sub, &subtask_handle[i], def::system::task_cpu_midi_sub);
  }
#endif
}

void task_midi_t::task_func(task_midi_t* me)
{
#if defined (M5UNIFIED_PC_BUILD)
  for (;;) {
    M5.delay(1);
  }
#else
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    auto portc_setting = system_registry.midi_port_setting.getPortCMIDI();
    bool portc_out = portc_setting & def::command::ex_midi_mode_t::midi_output;
    bool portc_in  = portc_setting & def::command::ex_midi_mode_t::midi_input;
    portc_midi_transport.setEnable(portc_out, portc_in);
#ifdef MIDI_TRANSPORT_BLE_HPP
    auto ble_setting = system_registry.midi_port_setting.getBLEMIDI();
    bool ble_out = ble_setting & def::command::ex_midi_mode_t::midi_output;
    bool ble_in  = ble_setting & def::command::ex_midi_mode_t::midi_input;
    ble_midi_transport.setEnable(ble_out, ble_in);
#endif
    for (int i = 0; i < max_subtask; ++i) {
      xTaskNotifyGive(subtask_handle[i]);
    }
  }
#endif
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns
