// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#ifndef KANPLAY_TASK_PORT_A_HPP
#define KANPLAY_TASK_PORT_A_HPP

/*
task_port_a は 外部ポートA (主にI2C用途)を使用するタスクです。
*/

namespace kanplay_ns {
//-------------------------------------------------------------------------
class task_port_a_t {
public:
    bool start(void);
private:
    static void task_func(task_port_a_t* me);
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
