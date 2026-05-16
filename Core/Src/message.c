//
// Created by yichu on 2026/5/16.
//
#include "message.h"
void Net_MessageBuildMessage(Net_SendMessageType *message,char *api_key,char *data) {
    message->curl = "https://api.deepseek.com/chat/completions";
    message->header1 = "Content-Type: application/json";
    message->header2 = (char*)malloc((sizeof (char))*100);
    sprintf(message->header2, "Authorization: Bearer %s", api_key);
    message->data = data;
}

void Net_MessageFree(Net_SendMessageType *message) {
    free(message->header2);
    free(message->data);
}

char* Net_MessageBuildData(char *content,uint16_t size) {
    uint16_t data_size = size + 500;
    char *data = (char*)malloc((sizeof (char))*data_size);
    sprintf(data,"{\"model\":\"deepseek-v4-flash\",\"messages\":[{\"role\":\"system\",\"content\": \"You are a helpful assistant,you need to return message like \"time:xxx;happened:yyy;EOF;\",the key words you can use only include\\\"time happened\\\",end an envent with\\\"EOF;\\\".Don't use\\n,represent BC with\\\"-\\\"in the head of number.if you search unsuccessfully,just return\\\"EOF;\\\" directly\"},{\"role\":\"user\",\"content\":\"%s\"}],\"thinking\":{\"type\":\"disabled\"},\"stream\":false}",content);
    return data;
}


//这几个函数就交给你了，社长
void Net_SendMessage(const Net_SendMessageType *message) {

}

void Net_ReceiveMessage(Net_ResponseType *response) {

}

void Net_Init() {

}