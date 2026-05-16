//
// Created by yichu on 2026/5/16.
//

#include "message.h"
#include "main.h"

#include <string.h>

#ifndef NET_UART_INSTANCE
#define NET_UART_INSTANCE USART1
#endif

#ifndef NET_UART_BAUDRATE
#define NET_UART_BAUDRATE 115200U
#endif

#ifndef NET_WIFI_SSID
#define NET_WIFI_SSID ""
#endif

#ifndef NET_WIFI_PASSWORD
#define NET_WIFI_PASSWORD ""
#endif

#ifndef NET_AT_BUFFER_SIZE
#define NET_AT_BUFFER_SIZE 512U
#endif

#ifndef NET_HTTP_REQUEST_BUFFER_SIZE
#define NET_HTTP_REQUEST_BUFFER_SIZE 4096U
#endif

#ifndef NET_RESPONSE_BUFFER_SIZE
#define NET_RESPONSE_BUFFER_SIZE 4096U
#endif

#define NET_UART_TIMEOUT_MS 1000U
#define NET_AT_TIMEOUT_MS 3000U
#define NET_WIFI_TIMEOUT_MS 15000U
#define NET_CONNECT_TIMEOUT_MS 10000U
#define NET_RESPONSE_TIMEOUT_MS 30000U
#define NET_RESPONSE_IDLE_TIMEOUT_MS 1200U

typedef struct {
    char host[96];
    char path[160];
    uint16_t port;
    uint8_t ssl;
} Net_EndpointType;

static USART_TypeDef *net_usart = NET_UART_INSTANCE;
static char net_at_buffer[NET_AT_BUFFER_SIZE];
static char net_http_request[NET_HTTP_REQUEST_BUFFER_SIZE];
static char net_response_buffer[NET_RESPONSE_BUFFER_SIZE];

static void Net_UARTLowLevelInit(void);
static uint8_t Net_UARTSendString(const char *text);
static uint8_t Net_UARTSendByte(uint8_t byte, uint32_t timeout_ms);
static uint8_t Net_UARTReceiveByte(uint8_t *byte, uint32_t timeout_ms);
static uint16_t Net_UARTRead(char *buffer, uint16_t capacity, uint32_t timeout_ms, uint32_t idle_timeout_ms);
static uint8_t Net_SendAT(const char *command, const char *expect, uint32_t timeout_ms);
static uint8_t Net_ParseUrl(const char *url, Net_EndpointType *endpoint);
static uint8_t Net_StartConnection(const Net_EndpointType *endpoint);
static uint16_t Net_ExtractIPDPayload(char *buffer, uint16_t length);
static uint16_t Net_JsonEscapedSize(const char *text);
static void Net_WriteJsonEscaped(char *dest, const char *src);

void Net_MessageBuildMessage(Net_SendMessageType *message, char *api_key, char *data) {
    if (message == NULL) {
        return;
    }

    message->curl = "https://api.deepseek.com/chat/completions";
    message->header1 = "Content-Type: application/json";
    message->header2 = NULL;
    message->data = data;

    if (api_key != NULL) {
        const char *prefix = "Authorization: Bearer ";
        size_t header_size = strlen(prefix) + strlen(api_key) + 1U;
        message->header2 = (char *)malloc(header_size);
        if (message->header2 != NULL) {
            snprintf(message->header2, header_size, "%s%s", prefix, api_key);
        }
    }
}

void Net_MessageFree(Net_SendMessageType *message) {
    if (message == NULL) {
        return;
    }

    free(message->header2);
    free(message->data);
    message->header2 = NULL;
    message->data = NULL;
}

char *Net_MessageBuildData(char *content, uint16_t size) {
    (void)size;

    if (content == NULL) {
        content = "";
    }

    const char *prefix =
        "{\"model\":\"deepseek-v4-flash\","
        "\"messages\":["
        "{\"role\":\"system\",\"content\":\""
        "You are a helpful assistant. Return only: time:xxx;happened:yyy;EOF;. "
        "Allowed keys: time happened. End every event with EOF;. Do not use newline. "
        "Use leading - for BC years. If search fails, return EOF; directly."
        "\"},"
        "{\"role\":\"user\",\"content\":\"";
    const char *suffix =
        "\"}],"
        "\"thinking\":{\"type\":\"disabled\"},"
        "\"stream\":false}";

    uint16_t escaped_size = Net_JsonEscapedSize(content);
    size_t data_size = strlen(prefix) + escaped_size + strlen(suffix) + 1U;
    char *data = (char *)malloc(data_size);
    if (data == NULL) {
        return NULL;
    }

    size_t prefix_size = strlen(prefix);
    memcpy(data, prefix, prefix_size);
    Net_WriteJsonEscaped(data + prefix_size, content);
    snprintf(data + prefix_size + escaped_size, strlen(suffix) + 1U, "%s", suffix);
    return data;
}

void Net_SendMessage(const Net_SendMessageType *message) {
    if (message == NULL || message->curl == NULL || message->header1 == NULL ||
        message->header2 == NULL || message->data == NULL) {
        return;
    }

    Net_EndpointType endpoint;
    if (!Net_ParseUrl(message->curl, &endpoint)) {
        return;
    }

    if (!Net_StartConnection(&endpoint)) {
        return;
    }

    size_t data_len = strlen(message->data);
    int request_len = snprintf(
        net_http_request,
        sizeof(net_http_request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "%s\r\n"
        "%s\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n"
        "\r\n"
        "%s",
        endpoint.path,
        endpoint.host,
        message->header1,
        message->header2,
        (unsigned int)data_len,
        message->data);

    if (request_len <= 0 || (size_t)request_len >= sizeof(net_http_request)) {
        return;
    }

    snprintf(net_at_buffer, sizeof(net_at_buffer), "AT+CIPSEND=%u", (unsigned int)request_len);
    if (!Net_SendAT(net_at_buffer, ">", NET_AT_TIMEOUT_MS)) {
        return;
    }

    Net_UARTSendString(net_http_request);
}

void Net_ReceiveMessage(Net_ResponseType *response) {
    if (response == NULL) {
        return;
    }

    memset(net_response_buffer, 0, sizeof(net_response_buffer));
    uint16_t length = Net_UARTRead(
        net_response_buffer,
        sizeof(net_response_buffer),
        NET_RESPONSE_TIMEOUT_MS,
        NET_RESPONSE_IDLE_TIMEOUT_MS);
    length = Net_ExtractIPDPayload(net_response_buffer, length);

    response->size = length;
    response->data = net_response_buffer;
}

void Net_Init(void) {
    Net_UARTLowLevelInit();

    HAL_Delay(500U);
    Net_SendAT("AT", "OK", NET_AT_TIMEOUT_MS);
    Net_SendAT("ATE0", "OK", NET_AT_TIMEOUT_MS);
    Net_SendAT("AT+CWMODE=1", "OK", NET_AT_TIMEOUT_MS);
    Net_SendAT("AT+CIPMUX=0", "OK", NET_AT_TIMEOUT_MS);
    Net_SendAT("AT+CIPMODE=0", "OK", NET_AT_TIMEOUT_MS);

    if (NET_WIFI_SSID[0] != '\0') {
        snprintf(net_at_buffer, sizeof(net_at_buffer), "AT+CWJAP=\"%s\",\"%s\"", NET_WIFI_SSID, NET_WIFI_PASSWORD);
        Net_SendAT(net_at_buffer, "WIFI GOT IP", NET_WIFI_TIMEOUT_MS);
    }
}

static void Net_UARTLowLevelInit(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
    uint32_t baud_div = (pclk2 + (NET_UART_BAUDRATE / 2U)) / NET_UART_BAUDRATE;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    net_usart->CR1 = 0U;
    net_usart->CR2 = 0U;
    net_usart->CR3 = 0U;
    net_usart->BRR = baud_div;
    net_usart->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static uint8_t Net_UARTSendString(const char *text) {
    if (text == NULL) {
        return 0U;
    }

    while (*text != '\0') {
        if (!Net_UARTSendByte((uint8_t)*text, NET_UART_TIMEOUT_MS)) {
            return 0U;
        }
        text++;
    }

    return 1U;
}

static uint8_t Net_UARTSendByte(uint8_t byte, uint32_t timeout_ms) {
    uint32_t start_tick = HAL_GetTick();

    while ((net_usart->SR & USART_SR_TXE) == 0U) {
        if ((HAL_GetTick() - start_tick) >= timeout_ms) {
            return 0U;
        }
    }

    net_usart->DR = byte;

    start_tick = HAL_GetTick();
    while ((net_usart->SR & USART_SR_TC) == 0U) {
        if ((HAL_GetTick() - start_tick) >= timeout_ms) {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t Net_UARTReceiveByte(uint8_t *byte, uint32_t timeout_ms) {
    uint32_t start_tick = HAL_GetTick();

    if (byte == NULL) {
        return 0U;
    }

    while ((net_usart->SR & USART_SR_RXNE) == 0U) {
        if ((net_usart->SR & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) != 0U) {
            (void)net_usart->SR;
            (void)net_usart->DR;
        }

        if ((HAL_GetTick() - start_tick) >= timeout_ms) {
            return 0U;
        }
    }

    *byte = (uint8_t)(net_usart->DR & 0xFFU);
    return 1U;
}

static uint16_t Net_UARTRead(char *buffer, uint16_t capacity, uint32_t timeout_ms, uint32_t idle_timeout_ms) {
    if (buffer == NULL || capacity == 0U) {
        return 0U;
    }

    uint16_t length = 0U;
    uint32_t start_tick = HAL_GetTick();
    uint32_t last_rx_tick = start_tick;
    uint8_t got_byte = 0U;

    while (length < (uint16_t)(capacity - 1U)) {
        uint8_t byte = 0U;
        if (Net_UARTReceiveByte(&byte, 20U)) {
            buffer[length++] = (char)byte;
            buffer[length] = '\0';
            got_byte = 1U;
            last_rx_tick = HAL_GetTick();

            if (strstr(buffer, "\r\nCLOSED") != NULL || strstr(buffer, "CLOSED\r\n") != NULL) {
                break;
            }
        } else {
            uint32_t now = HAL_GetTick();
            if ((now - start_tick) >= timeout_ms) {
                break;
            }
            if (got_byte && (now - last_rx_tick) >= idle_timeout_ms) {
                break;
            }
        }
    }

    buffer[length] = '\0';
    return length;
}

static uint8_t Net_SendAT(const char *command, const char *expect, uint32_t timeout_ms) {
    memset(net_at_buffer, 0, sizeof(net_at_buffer));

    if (!Net_UARTSendString(command) || !Net_UARTSendString("\r\n")) {
        return 0U;
    }

    if (expect == NULL) {
        return 1U;
    }

    Net_UARTRead(net_at_buffer, sizeof(net_at_buffer), timeout_ms, 200U);
    return strstr(net_at_buffer, expect) != NULL;
}

static uint8_t Net_ParseUrl(const char *url, Net_EndpointType *endpoint) {
    if (url == NULL || endpoint == NULL) {
        return 0U;
    }

    memset(endpoint, 0, sizeof(*endpoint));
    endpoint->port = 80U;
    endpoint->ssl = 0U;

    const char *cursor = url;
    if (strncmp(cursor, "https://", 8U) == 0) {
        endpoint->ssl = 1U;
        endpoint->port = 443U;
        cursor += 8U;
    } else if (strncmp(cursor, "http://", 7U) == 0) {
        cursor += 7U;
    }

    const char *path = strchr(cursor, '/');
    const char *host_end = path == NULL ? cursor + strlen(cursor) : path;
    const char *port = strchr(cursor, ':');

    if (port != NULL && port < host_end) {
        endpoint->port = (uint16_t)strtoul(port + 1, NULL, 10);
        host_end = port;
    }

    size_t host_len = (size_t)(host_end - cursor);
    if (host_len == 0U || host_len >= sizeof(endpoint->host)) {
        return 0U;
    }

    memcpy(endpoint->host, cursor, host_len);
    endpoint->host[host_len] = '\0';

    if (path == NULL) {
        snprintf(endpoint->path, sizeof(endpoint->path), "/");
    } else if (strlen(path) < sizeof(endpoint->path)) {
        snprintf(endpoint->path, sizeof(endpoint->path), "%s", path);
    } else {
        return 0U;
    }

    return 1U;
}

static uint8_t Net_StartConnection(const Net_EndpointType *endpoint) {
    if (endpoint == NULL) {
        return 0U;
    }

    Net_SendAT("AT+CIPCLOSE", NULL, 500U);
    HAL_Delay(100U);

    snprintf(
        net_at_buffer,
        sizeof(net_at_buffer),
        "AT+CIPSTART=\"%s\",\"%s\",%u",
        endpoint->ssl ? "SSL" : "TCP",
        endpoint->host,
        endpoint->port);

    memset(net_response_buffer, 0, sizeof(net_response_buffer));
    if (!Net_UARTSendString(net_at_buffer) || !Net_UARTSendString("\r\n")) {
        return 0U;
    }

    Net_UARTRead(net_response_buffer, sizeof(net_response_buffer), NET_CONNECT_TIMEOUT_MS, 300U);
    return strstr(net_response_buffer, "CONNECT") != NULL ||
           strstr(net_response_buffer, "ALREADY CONNECTED") != NULL ||
           strstr(net_response_buffer, "OK") != NULL;
}

static uint16_t Net_ExtractIPDPayload(char *buffer, uint16_t length) {
    if (buffer == NULL || length == 0U) {
        return 0U;
    }

    char *read_cursor = buffer;
    char *write_cursor = buffer;
    uint8_t found_ipd = 0U;

    while (read_cursor < buffer + length) {
        char *ipd = strstr(read_cursor, "+IPD,");
        if (ipd == NULL) {
            break;
        }

        char *colon = strchr(ipd, ':');
        if (colon == NULL || colon >= buffer + length) {
            break;
        }

        char *len_start = colon;
        while (len_start > ipd && *len_start != ',') {
            len_start--;
        }
        if (*len_start != ',') {
            break;
        }
        len_start++;

        uint32_t payload_len = strtoul(len_start, NULL, 10);
        char *payload = colon + 1;
        uint32_t available_len = (uint32_t)((buffer + length) - payload);
        if (payload_len > available_len) {
            payload_len = available_len;
        }

        memmove(write_cursor, payload, payload_len);
        write_cursor += payload_len;
        read_cursor = payload + payload_len;
        found_ipd = 1U;
    }

    if (!found_ipd) {
        buffer[length] = '\0';
        return length;
    }

    *write_cursor = '\0';
    return (uint16_t)(write_cursor - buffer);
}

static uint16_t Net_JsonEscapedSize(const char *text) {
    uint16_t length = 0U;

    while (text != NULL && *text != '\0') {
        switch (*text) {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                length += 2U;
                break;
            default:
                length += 1U;
                break;
        }
        text++;
    }

    return length;
}

static void Net_WriteJsonEscaped(char *dest, const char *src) {
    while (src != NULL && *src != '\0') {
        switch (*src) {
            case '\"':
                *dest++ = '\\';
                *dest++ = '\"';
                break;
            case '\\':
                *dest++ = '\\';
                *dest++ = '\\';
                break;
            case '\b':
                *dest++ = '\\';
                *dest++ = 'b';
                break;
            case '\f':
                *dest++ = '\\';
                *dest++ = 'f';
                break;
            case '\n':
                *dest++ = '\\';
                *dest++ = 'n';
                break;
            case '\r':
                *dest++ = '\\';
                *dest++ = 'r';
                break;
            case '\t':
                *dest++ = '\\';
                *dest++ = 't';
                break;
            default:
                *dest++ = *src;
                break;
        }
        src++;
    }
    *dest = '\0';
}
