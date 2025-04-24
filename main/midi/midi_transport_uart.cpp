
#include "midi_transport_uart.hpp"

#if __has_include(<driver/uart.h>)

#include <driver/uart.h>

namespace midi_driver {

//----------------------------------------------------------------

MIDI_Transport_UART::~MIDI_Transport_UART()
{
  end();
}

bool MIDI_Transport_UART::begin(void)
{
  const uart_port_t uart_num = (uart_port_t)_config.uart_port_num;

  uart_config_t uart_config = {
      .baud_rate = _config.baud_rate,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 58,
      .source_clk = UART_SCLK_APB
  };

  // Configure UART parameters
  esp_err_t err;
  err = uart_param_config(uart_num, &uart_config);
  // M5_LOGD("uart_midi:uart_param_config: %d", err);
  if (err != ESP_OK) {
    return err;
  }

// Install UART driver using an event queue here
  err = uart_driver_install(uart_num, _config.buffer_size_rx, _config.buffer_size_tx, 0, nullptr, 0);
  // M5_LOGD("uart_midi:uart_driver_install: %d", err);

  _is_begin = true;

  // setEnable(_tx_enable, _rx_enable);
  return err;
}

void MIDI_Transport_UART::end(void)
{
  if (_is_begin) {
    _is_begin = false;
    uart_driver_delete((uart_port_t)_config.uart_port_num);
  }
}

size_t MIDI_Transport_UART::write(const uint8_t* data, size_t length)
{
// printf("uart_midi:write: %d\n", length);
  if (_tx_enable == false) { return 0; }
  uart_port_t uart_num = (uart_port_t) _config.uart_port_num;
  return uart_write_bytes(uart_num, data, length);
}

size_t MIDI_Transport_UART::read(uint8_t* data, size_t length)
{
  if (_rx_enable == false) { return 0; }
  uart_port_t uart_num = (uart_port_t) _config.uart_port_num;
  return uart_read_bytes(uart_num, data, length, 1);
}

void MIDI_Transport_UART::setEnable(bool tx_enable, bool rx_enable)
{
  if (_tx_enable == tx_enable && _rx_enable == rx_enable) { return; }
  MIDI_Transport::setEnable(tx_enable, rx_enable);
  int pin_tx = tx_enable ? _config.pin_tx : -1;
  int pin_rx = rx_enable ? _config.pin_rx : -1;
  // esp_err_t err = 
  uart_set_pin((uart_port_t)_config.uart_port_num, pin_tx, pin_rx, -1, -1);
  // M5_LOGD("uart_midi:uart_set_pin: %d", err);
}

//----------------------------------------------------------------

} // namespace midi_driver

#endif
