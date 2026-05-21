//
// Created by yichu on 2026/5/16.
//

#ifndef STM32_HISTORYLINE_CONTROL_H
#define STM32_HISTORYLINE_CONTROL_H

//按键事件状态机
enum ControlModeType{
    DEFAULT,
    LEFT,
    RIGHT,
    INTERACTION
};
volatile extern enum ControlModeType control_mode;
void ControlInit();
void ControlMain();

#endif //STM32_HISTORYLINE_CONTROL_H
