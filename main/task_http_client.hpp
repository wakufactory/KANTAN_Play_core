// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#ifndef KANPLAY_TASK_HTTP_CLIENT_HPP
#define KANPLAY_TASK_HTTP_CLIENT_HPP

/*
task_http_client は外部サーバにhttp接続したり、OTAファームウェア更新を実施するタスクです。
*/

namespace kanplay_ns {
//-------------------------------------------------------------------------
class task_http_client_t {
public:
  void start(void);
  void exec_ota(const char* json_url);
  enum request_t {
    request_none,
    request_ota,
  };
private:
  static void task_func(task_http_client_t* me);
  request_t _request = request_none;
  const char* _ota_json_url = nullptr;
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
