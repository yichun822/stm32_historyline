//
// Created by yichu on 2026/5/16.
//
#include "control.h"
#include <stdlib.h>
#include "main.h"
#include "history_object.h"
volatile enum ControlModeType control_mode;
void ControlInit() {
    control_mode=DEFAULT;
}
void active_manager() {
    static uint32_t last_time=0;
    if (last_time+20>HAL_GetTick())return; //软件消抖
    last_time=HAL_GetTick();
    if (HAL_GPIO_ReadPin(LEFT_BUTTON_GPIO_Port,LEFT_BUTTON_Pin)==GPIO_PIN_RESET)control_mode=LEFT;
    else if (HAL_GPIO_ReadPin(RIGHT_BUTTON_GPIO_Port,RIGHT_BUTTON_Pin)==GPIO_PIN_RESET)control_mode=RIGHT;
    else if (HAL_GPIO_ReadPin(INTERACTION_BUTTON_GPIO_Port,INTERACTION_BUTTON_Pin)==GPIO_PIN_RESET)control_mode=INTERACTION;
    else control_mode=DEFAULT;
}

void ControlMain() {
    active_manager();
    if (control_mode==LEFT) {

    }
    else if (control_mode==RIGHT) {

    }
    else if (control_mode==INTERACTION) {

    }
    else {

    }
}
