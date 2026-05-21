//
// Created by yichu on 2026/5/17.
//

#ifndef STM32_HISTORYLINE_HISTORY_OBJECT_H
#define STM32_HISTORYLINE_HISTORY_OBJECT_H

#include <stdint.h>
//对message的临时存储
typedef struct History_Object History_Object;

struct History_Object {
    int16_t time;
    char *event;
    uint16_t strlen;
    History_Object *next;
};

void History_ObjectBuild(History_Object *this,int16_t time,char *event,uint16_t strlen,History_Object *next);
void History_ObjectDestroy(History_Object *this);

#endif //STM32_HISTORYLINE_HISTORY_OBJECT_H
