//
// Created by yichu on 2026/5/16.
//

#ifndef STM32_HISTORYLINE_MESSAGE_H
#define STM32_HISTORYLINE_MESSAGE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *curl;
    const char *header1;
    char *header2;
    char *data;
} Net_SendMessageType;

typedef struct {
    uint16_t size;
    char *data;
} Net_ResponseType;

void Net_MessageBuildMessage(Net_SendMessageType *message, char *api_key, char *data);
void Net_MessageFree(Net_SendMessageType *message);
char *Net_MessageBuildData(char *content, uint16_t size);

void Net_SendMessage(const Net_SendMessageType *message);
void Net_ReceiveMessage(Net_ResponseType *response);
void Net_Init(void);

#endif //STM32_HISTORYLINE_MESSAGE_H
