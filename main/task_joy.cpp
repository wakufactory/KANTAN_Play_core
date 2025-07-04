// SPDX-License-Identifier: MIT
// Copyright (c) 2025 InstaChord Corp.

#include <M5Unified.h>
#include <stdio.h>
#include "task_joy.hpp"
#include "system_registry.hpp"
#include "common_define.hpp"

#define JOY_I2C_ADDR 0x63 

namespace kanplay_ns {
//-------------------------------------------------------------------------

void task_joy_t::start(void)
{
#if defined (M5UNIFIED_PC_BUILD)
    auto thread = SDL_CreateThread((SDL_ThreadFunction)task_func, "joy", this);
#else
    TaskHandle_t handle = nullptr;

    M5.Ex_I2C.begin();

    _exists = checkDevice();  // 初回接続確認
 
    xTaskCreatePinnedToCore((TaskFunction_t)task_func, "joy", 4096, this,
                            def::system::task_priority_joy, &handle, def::system::task_cpu_joy);
#endif
}
 
uint8_t jx ;
void task_joy_t::task_func(task_joy_t* me)
{
    int lc = -1 ;
    int lz = -1 ;
    int lastchord = 0 ; 
    def::command::command_param_t command = {
        def::command::command_t::chord_degree, // コード選択 1~7
        1  // 1~7
    };
    for (;;) {
        if(me->_exists) {
            // ジョイスティック入力処理を実装
            int8_t axis[2] ;
            if(!M5.Ex_I2C.readRegister(JOY_I2C_ADDR, 0x60, (uint8_t *)axis,2,400000)) {
                me->_exists = false; // デバイスが見つからない場合はフラグを下げる
                continue ;
            }
            int8_t x = axis[0] ; // X軸
            int8_t y = axis[1] ; // Y軸
            uint8_t z = M5.Ex_I2C.readRegister8(JOY_I2C_ADDR, 0x20, 400000);

            if(lz != z) {
                if(z==0) {
                    // ボタンが押された
                    printf("BP\n");
                    def::command::command_param_t command = {
                        def::command::command_t::autoplay_switch, // オートプレイのスイッチ
                        def::command::autoplay_switch_t::autoplay_toggle // トグル
                    };
                    system_registry.operator_command.addQueue(command);
                }  
                lz = z ; 
            }
            float th = atan2f((float)y, (float)x)/M_PI+1; // 
            float r = sqrtf((float)(x * x + y * y));  
            float t = fmod(th*4+3.5,1.0) ;
            int n = (t>0.3 && t < 0.7) ? ((int)(th*4+3.5))%8 : -1; // 0~7 
            if(r<100 ) {
                n = -1; // 中央
            }
            if(lc != n) {
                if(n >= 0) {
                    printf("on  %f %d\n",th,n  ) ;
                    if(n>0) {
                        command.param = n; // 1~7
                        system_registry.operator_command.addQueue( command,true );
                    }  
                    lastchord = n ;  
                } else { 
                    printf("off %d\n",n  ) ;
                    if(lastchord > 0) {
                        command.param = lastchord; // 1~7
                        system_registry.operator_command.addQueue( command,false  );
                    }
                }
                lc = n ;
            }
        } else {
            //デバイスが見つからない場合は、再度確認
            me->_exists = me->checkDevice();
            if(me->_exists) printf("Joy device found\n");
        }
        delay(10);
    }
}
bool task_joy_t::checkDevice(void) {
    uint8_t dummy;
    return M5.Ex_I2C.scanID(JOY_I2C_ADDR) && M5.Ex_I2C.readRegister(JOY_I2C_ADDR, 0xff, &dummy, 1,400000);
}
//-------------------------------------------------------------------------
}; // namespace kanplay_ns
