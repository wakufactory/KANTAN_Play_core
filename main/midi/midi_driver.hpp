// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#ifndef MIDI_DRIVER_HPP
#define MIDI_DRIVER_HPP

#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

namespace midi_driver {

  // MIDI Message structure
  struct MIDI_Message {
    std::vector<uint8_t> data;
    union {
      uint8_t status;
      struct {
        uint8_t channel : 4;
        uint8_t type : 4;
      };
    };
  };
/*
  // MIDI Encoder class
  class MIDI_Encoder {
    std::vector<uint8_t> _data;
    uint8_t _runningStatus;
  public:
    MIDI_Encoder() : _runningStatus(0) {}
    virtual ~MIDI_Encoder() = default;
    void clearRunningStatus(void) { _runningStatus = 0; }
    void clear(void) { _data.clear(); }
    const uint8_t* getBuffer(void) const { return _data.data(); }
    size_t getLength(void) const { return _data.size(); }
    void pushMessage(const MIDI_Message& message);
    void pushMessage(uint8_t status_byte, uint8_t data1 = 0, uint8_t data2 = 0);
  };
//*/
  // MIDI Decoder class
  class MIDI_Decoder {
    std::vector<uint8_t> _data;
    uint8_t _runningStatus;
  public:
    MIDI_Decoder() : _runningStatus(0) {}
    virtual ~MIDI_Decoder() = default;
    void clear(void) { _data.clear(); }

    void addData(const uint8_t* data, size_t length) {
      _data.insert(_data.end(), data, data + length);
// printf("data.size:%d\n", _data.size());
    }
    bool popMessage(MIDI_Message* message);
  };

  // Abstract base class for MIDI transport
  class MIDI_Transport {
  protected:
    bool _tx_enable = false;
    bool _rx_enable = false;
  public:
    virtual ~MIDI_Transport() = default;

    // Pure virtual functions to be implemented by derived classes
    virtual bool begin(void) = 0;
    virtual void end(void) = 0;
    virtual size_t write(const uint8_t* data, size_t length) = 0;
    virtual size_t read(uint8_t* data, size_t length) = 0;

    bool getEnableTx(void) const { return _tx_enable; }
    bool getEnableRx(void) const { return _rx_enable; }
    void setEnableTx(bool enable) { setEnable(enable, _rx_enable); }
    void setEnableRx(bool enable) { setEnable(_tx_enable, enable); }
    virtual void setEnable(bool tx_enable, bool rx_enable) {
      _tx_enable = tx_enable;
      _rx_enable = rx_enable;
    }
  };

  // MIDI Driver class
  class MIDIDriver {
    std::vector<uint8_t> _send_data;
    uint8_t _send_runningStatus;
  public:
    MIDIDriver(MIDI_Transport* transport) : _transport(transport) {}
    virtual ~MIDIDriver() = default;

    void setSendBufferSize(size_t size) { _send_buffer_size = size; }

    bool getEnableTx(void) const { return _transport->getEnableTx(); }
    bool getEnableRx(void) const { return _transport->getEnableRx(); }
    void setEnableTx(bool enable) { _transport->setEnableTx(enable); }
    void setEnableRx(bool enable) { _transport->setEnableRx(enable); }

    void sendMessage(uint8_t status_byte, uint8_t data1, uint8_t data2);

    void sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
      sendMessage(0x90 | channel, note, velocity);
    }
    void sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
      sendMessage(0x80 | channel, note, velocity);
    }
    void sendControlChange(uint8_t channel, uint8_t control, uint8_t value) {
      sendMessage(0xB0 | channel, control, value);
    }
    void sendProgramChange(uint8_t channel, uint8_t program) {
      sendMessage(0xC0 | channel, program, 0);
    }

    bool sendFlush(void) {
      if (_send_data.size() == 0) { return true; }
      _send_runningStatus = 0;
      bool result = _transport->write(_send_data.data(), _send_data.size());
      _send_data.clear();
      return result;
    }
    void receive(void) {
      uint8_t data[32];
      int len = _transport->read(data, sizeof(data));
// if (len != 0) {
// printf("uart_midi:receive: %d\n", len);
// }
      if (len > 0) {
        _decoder.addData(data, len);
      }
    }
    bool receiveMessage(MIDI_Message* message) {
      return _decoder.popMessage(message);
    }

    void setEnable(bool tx_enable, bool rx_enable) {
      _transport->setEnable(tx_enable, rx_enable);
    }

  private:
    MIDI_Transport* _transport;
    // MIDI_Encoder _encoder;
    MIDI_Decoder _decoder;
    size_t _send_buffer_size = 64;
  };

};

#endif
