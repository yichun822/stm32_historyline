//
// Created by yichu on 2026/5/17.
//
#include "history_object.h"

#include <stdlib.h>


void History_ObjectBuild(History_Object *this,int16_t time,char *event,uint16_t strlen,History_Object *next){
    this->time = time;
    this->event = event;
    this->strlen = strlen;
    this->next = next;
}

void History_ObjectDestroy(History_Object *this) {
    while (this->next!=NULL) {
        History_Object *past =this;
        this = this->next;
        free(past);
    }
    free(this);
}
