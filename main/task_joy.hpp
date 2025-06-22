// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#ifndef KANPLAY_TASK_JOY_HPP
#define KANPLAY_TASK_JOY_HPP

/*
 task_joy はジョイスティック入力を処理するタスクです。
*/

namespace kanplay_ns {
//-------------------------------------------------------------------------
class task_joy_t {
public:
    void start(void);
private:
    static void task_func(task_joy_t* me);
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
