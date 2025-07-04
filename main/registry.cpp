// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#include "registry.hpp"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if __has_include(<malloc.h>)
#include <malloc.h>
#endif

#include <M5Unified.h>

#if defined (__WIN32__) || defined (_WIN32) || defined (__CYGWIN__)
  #define aligned_alloc(align,size) _aligned_malloc(size,align)
#endif

namespace kanplay_ns {

//-------------------------------------------------------------------------

#if __has_include (<freertos/freertos.h>)
void registry_base_t::setNotifyTaskHandle(TaskHandle_t handle)
{
  if (_task_handle != nullptr) {
    M5_LOGE("task handle already set");
    return;
  }
  _task_handle = handle;
}
#endif

registry_base_t::registry_base_t(uint16_t history_count)
: _history_code { 0 }
, _history_count(history_count)
{
  _history = nullptr;
}

registry_base_t::~registry_base_t(void)
{ if (_history != nullptr) { m5gfx::heap_free(_history); } }

void registry_base_t::init(bool psram)
{
  if (_history_count) {
    size_t history_size = _history_count * sizeof(history_t);
    void* ptr = nullptr;
    if (psram) {
      ptr = m5gfx::heap_alloc_psram(history_size);
    }
    if (ptr == nullptr) {
      ptr = m5gfx::heap_alloc(history_size);
    }
    memset(ptr, 0xFF, history_size);
    _history = (history_t*)ptr;
  }
}

void registry_base_t::_addHistory(uint16_t index, uint32_t value, data_size_t data_size)
{
  uint16_t history_index = _history_code & 0xFFFF;
  uint8_t history_seq = _history_code >> 16;
  if (_history != nullptr) {
    _history[history_index].value = value;
    _history[history_index].index = index;
    _history[history_index].data_size = data_size;
    _history[history_index].seq = history_seq;
  }
  if (++history_index >= _history_count)
  {
    history_index = 0;
    history_seq++;
  }
  _history_code = history_index | history_seq << 16;
}


// 変更履歴を取得する
const registry_base_t::history_t* registry_base_t::getHistory(history_code_t &code)
{
  if (_history_code == code || _history == nullptr) {
    return nullptr;
  }
  auto index = code & 0xFFFF;
  if (index >= _history_count) {
    M5_LOGE("history index out of range : %d", index);
    return nullptr;
  }
  uint8_t seq = code >> 16;
  if (seq != _history[index].seq) {
    M5_LOGW("history seq looping : request:%08x  seq:%d  data seq:%d", code, seq, _history[index].seq);
  }
  auto res = &_history[index];
  if (++index >= _history_count) {
    index = 0;
    ++seq;
  }
  code = index | (seq << 16);
  return res;
}


void registry_t::assign(const registry_t &src) {
  memcpy(_reg_data, src._reg_data, _registry_size);
  if (_history_count == 0) {
    _history_code += 1 << 16;
  }
  _execNotify();
}

registry_t::registry_t(uint16_t registry_size, uint16_t history_count, data_size_t data_size)
: registry_base_t(history_count)
{
  _registry_size = registry_size;
  _data_size = data_size;
  // _reg_data = malloc(registry_size);
  // memset(_reg_data, 0, registry_size);
}

registry_t::~registry_t(void)
{ if (_reg_data != nullptr) { m5gfx::heap_free(_reg_data); } }

void registry_t::init(bool psram)
{
  if (_reg_data != nullptr) {
    M5_LOGE("registry_t::init: already initialized");
    return;
  }
  registry_base_t::init(psram);
  if (psram) {
    _reg_data = (uint8_t*)m5gfx::heap_alloc_psram(_registry_size);
  } else {
    _reg_data = (uint8_t*)m5gfx::heap_alloc(_registry_size);
  }
  if (_reg_data) {
    memset(_reg_data, 0, _registry_size);
  }
}

void registry_base_t::set8(uint16_t index, uint8_t value, bool force_notify)
{
  _addHistory(index, value, data_size_t::DATA_SIZE_8);
  _execNotify();
}

void registry_base_t::set16(uint16_t index, uint16_t value, bool force_notify)
{
  _addHistory(index, value, data_size_t::DATA_SIZE_16);
  _execNotify();
}

void registry_base_t::set32(uint16_t index, uint32_t value, bool force_notify)
{
  _addHistory(index, value, data_size_t::DATA_SIZE_32);
  _execNotify();
}

void registry_t::set8(uint16_t index, uint8_t value, bool force_notify)
{
  if (index + 1 > _registry_size) {
    M5_LOGE("set8: index out of range : %d", index);
    assert(index < _registry_size && "set8: index out of range");
  }
  auto dst = &_reg_data_8[index];
  if (*dst != value || force_notify) {
    *dst = value;
    switch (_data_size) {
    default: return;
    case data_size_t::DATA_SIZE_8:
      _addHistory(index, value, data_size_t::DATA_SIZE_8);
      break;
    case data_size_t::DATA_SIZE_16:
      {
        index >>= 1;
        uint16_t v = _reg_data_16[index];
        _addHistory(index << 1, v, data_size_t::DATA_SIZE_16);
      }
      break;
    case data_size_t::DATA_SIZE_32:
      {
        index >>= 2;
        uint32_t v = _reg_data_32[index];
        _addHistory(index << 2, v, data_size_t::DATA_SIZE_32);
      }
      break;
    }
    _execNotify();
  }
}

void registry_t::set16(uint16_t index, uint16_t value, bool force_notify)
{
    if (index + 2 > _registry_size) {
        M5_LOGE("set16: index out of range : %d", index);
        return;
    }
    if (index & 1) {
        M5_LOGE("set16: alignment error : %d", index);
        return;
    }
    auto dst = &_reg_data_16[index >> 1];
    if (*dst != value || force_notify) {
        *dst = value;
        switch (_data_size) {
        default: return;
        case data_size_t::DATA_SIZE_16:
            _addHistory(index, value, data_size_t::DATA_SIZE_16);
            break;
        case data_size_t::DATA_SIZE_8:
            {
                uint8_t v = _reg_data_8[index];
                _addHistory(index, v, data_size_t::DATA_SIZE_8);
                v = _reg_data_8[++index];
                _addHistory(index, v, data_size_t::DATA_SIZE_8);
            }
            break;
        case data_size_t::DATA_SIZE_32:
            {
                index >>= 2;
                uint32_t v = _reg_data_32[index];
                _addHistory(index << 2, v, data_size_t::DATA_SIZE_32);
            }
        }
        _execNotify();
    }
}

void registry_t::set32(uint16_t index, uint32_t value, bool force_notify)
{
    if (index + 4 > _registry_size) {
        M5_LOGE("set32: index out of range : %d", index);
        return;
    }
    if (index & 3) {
        M5_LOGE("set32: alignment error : %d", index);
        return;
    }
    auto dst = &_reg_data_32[index >> 2];
    if (*dst != value || force_notify) {
        *dst = value;
        switch (_data_size) {
        default: return;
        case data_size_t::DATA_SIZE_32:
            _addHistory(index, value, data_size_t::DATA_SIZE_32);
            break;
        case data_size_t::DATA_SIZE_16:
            {
                uint16_t v = _reg_data_16[index >> 1];
                _addHistory(index, v, data_size_t::DATA_SIZE_16);
                index += 2;
                v = _reg_data_16[index >> 1];
                _addHistory(index, v, data_size_t::DATA_SIZE_16);
            }
            break;
        case data_size_t::DATA_SIZE_8:
            {
                uint8_t v = _reg_data_8[index];
                _addHistory(index, v, data_size_t::DATA_SIZE_8);
                v = _reg_data_8[++index];
                _addHistory(index, v, data_size_t::DATA_SIZE_8);
                v = _reg_data_8[++index];
                _addHistory(index, v, data_size_t::DATA_SIZE_8);
                v = _reg_data_8[++index];
                _addHistory(index, v, data_size_t::DATA_SIZE_8);
            }
            break;
        }
        _execNotify();
    }
}

uint8_t registry_t::get8(uint16_t index) const
{
    if (index + 1 > _registry_size) {
        M5_LOGE("get8: index out of range : %d", index);
        assert(index < _registry_size && "get8: index out of range");
    }
    return _reg_data_8[index];
}

uint16_t registry_t::get16(uint16_t index) const
{
    if (index + 2 > _registry_size) {
        M5_LOGE("get16: index out of range : %d", index);
        return 0;
    }
    if (index & 1) {
        M5_LOGE("get16: alignment error : %d", index);
        return 0;
    }
    return _reg_data_16[index >> 1];
}

uint32_t registry_t::get32(uint16_t index) const
{
    if (index + 4 > _registry_size) {
        M5_LOGE("get32: index out of range : %d", index);
        assert(index + 4 <= _registry_size && "get32: index out of range");
    }
    if (index & 3) {
        M5_LOGE("get32: alignment error : %d", index);
        return 0;
    }
    return _reg_data_32[index >> 2];
}

bool registry_t::operator==(const registry_t &rhs) const
{
    return memcmp(_reg_data, rhs._reg_data, _registry_size) == 0;
}

//-------------------------------------------------------------------------

void registry_map8_t::set8(uint16_t index, uint8_t value, bool force_notify)
{
  if (value == _default_value) {
    _data.erase(index);
  } else {
    _data[index] = value;
  }
  _addHistory(index, value, data_size_t::DATA_SIZE_8);
  _execNotify();
}

uint8_t registry_map8_t::get8(uint16_t index) const
{
  auto it = _data.find(index);
  if (it == _data.end()) {
    return _default_value;
  }
  return it->second;
}

void registry_map8_t::assign(const registry_map8_t &src)
{
  _data = src._data;
  if (_history_count == 0) {
    _history_code += 1 << 16;
  }
  _execNotify();
}

//-------------------------------------------------------------------------

void registry_map32_t::set32(uint16_t index, uint32_t value, bool force_notify)
{
  if (value == _default_value) {
    _data.erase(index);
  } else {
    _data[index] = value;
  }
  _addHistory(index, value, data_size_t::DATA_SIZE_8);
  _execNotify();
}

uint32_t registry_map32_t::get32(uint16_t index) const
{
  auto it = _data.find(index);
  if (it == _data.end()) {
    return _default_value;
  }
  return it->second;
}

void registry_map32_t::assign(const registry_map32_t &src)
{
  _data = src._data;
  if (_history_count == 0) {
    _history_code += 1 << 16;
  }
  _execNotify();
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns
