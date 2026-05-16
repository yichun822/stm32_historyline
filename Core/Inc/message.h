//
// Created by yichu on 2026/5/16.
//

#ifndef STM32_HISTORYLINE_MESSAGE_H
#define STM32_HISTORYLINE_MESSAGE_H


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef enum {
    NET_STATUS_OK = 0,
    NET_STATUS_INVALID_ARGUMENT = -1,
    NET_STATUS_NO_MEMORY = -2,
    NET_STATUS_BAD_URL = -3,
    NET_STATUS_TRANSPORT_ERROR = -4,
    NET_STATUS_RESPONSE_TOO_LARGE = -5
} Net_StatusType;

typedef struct {
    const char *curl;
    const char *header1;
    char *header2;
    char *data;
} Net_SendMessageType;

typedef struct {
    uint16_t size;
    char *data;
}Net_ResponseType;

void Net_MessageBuildMessage(Net_SendMessageType *message,char *api_key,char *data);
void Net_MessageFree(Net_SendMessageType *message);
void Net_ResponseFree(Net_ResponseType *response);
char* Net_MessageBuildData(char *content,uint16_t size);

void Net_SendMessage(const Net_SendMessageType *message);
void Net_ReceiveMessage(Net_ResponseType *response);
void Net_Init(void);
int16_t Net_GetLastError(void);

/* Override these transport hooks in the WiFi/TLS module.
 * Return 0 for init/open success. Send returns bytes written.
 * Receive returns bytes read, 0 when no more data, or a negative error.
 */
int Net_TransportInit(void);
int Net_TransportOpen(const char *host, uint16_t port, uint8_t use_tls);
int Net_TransportSend(const char *data, uint16_t size);
int Net_TransportReceive(char *data, uint16_t size, uint32_t timeout_ms);
void Net_TransportClose(void);

#endif //STM32_HISTORYLINE_MESSAGE_H
