// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#ifndef KANPLAY_TASK_I2S_HPP
#define KANPLAY_TASK_I2S_HPP

/*
task_i2s は I2Sオーディオ入出力を担当するタスクです。
 - かんぷれハードウェア内部の ES8388 とのI2S入出力
 - ソフトウェア音源とI2S入力のミキシングや取り込み

 - 生成したオーディオ信号データの取り出し（未実装）
*/

namespace kanplay_ns {
//-------------------------------------------------------------------------
class task_i2s_t {
public:
    bool start(void);

    void playRaw(float samplerate, const int16_t* src, size_t len, bool stereo);
private:
    static void task_func(task_i2s_t* me);
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
