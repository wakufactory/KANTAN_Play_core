#ifndef MIDI_TRANSPORT_UART_HPP
#define MIDI_TRANSPORT_UART_HPP

#include "midi_driver.hpp"

namespace midi_driver {

class MIDI_Transport_UART : public MIDI_Transport {
public:
  struct config_t {
    int baud_rate = 31250;
    uint16_t buffer_size_tx = 144;
    uint16_t buffer_size_rx = 144;
    uint8_t uart_port_num = 0;
    int8_t pin_tx = -1;
    int8_t pin_rx = -1;
  };

  MIDI_Transport_UART(void) = default;
  ~MIDI_Transport_UART();

  void setConfig(const config_t& config) { _config = config; }

  bool begin(void) override;
  void end(void) override;
  size_t write(const uint8_t* data, size_t length) override;
  size_t read(uint8_t* data, size_t length) override;
  void setEnable(bool tx_enable, bool rx_enable) override;

private:
  config_t _config;
  bool _is_begin;
};

} // namespace midi_driver

#endif // MIDI_TRANSPORT_UART_HPP
