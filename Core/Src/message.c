//
// Created by yichu on 2026/5/16.
//
#include "message.h"

#include <ctype.h>
#include <string.h>

#define NET_API_URL "https://api.deepseek.com/chat/completions"
#define NET_CONTENT_TYPE_HEADER "Content-Type: application/json"
#define NET_AUTH_HEADER_PREFIX "Authorization: Bearer "
#define NET_HTTP_RX_CHUNK_SIZE 256U
#define NET_HTTP_RESPONSE_MAX_SIZE 8192U
#define NET_HTTP_TIMEOUT_MS 30000U
#define NET_HOST_MAX_SIZE 96U
#define NET_PATH_MAX_SIZE 160U

#if defined(__GNUC__)
#define NET_WEAK __attribute__((weak))
#else
#define NET_WEAK
#endif

typedef struct {
    char host[NET_HOST_MAX_SIZE];
    char path[NET_PATH_MAX_SIZE];
    uint16_t port;
    uint8_t use_tls;
} Net_EndpointType;

static int16_t net_last_error = NET_STATUS_OK;
static uint8_t net_initialized = 0U;

static void Net_SetLastError(Net_StatusType status) {
    net_last_error = (int16_t)status;
}

NET_WEAK int Net_TransportInit(void) {
    return 0;
}

NET_WEAK int Net_TransportOpen(const char *host, uint16_t port, uint8_t use_tls) {
    (void)host;
    (void)port;
    (void)use_tls;
    return 0;
}

NET_WEAK int Net_TransportSend(const char *data, uint16_t size) {
    (void)data;
    (void)size;
    return -1;
}

NET_WEAK int Net_TransportReceive(char *data, uint16_t size, uint32_t timeout_ms) {
    (void)data;
    (void)size;
    (void)timeout_ms;
    return 0;
}

NET_WEAK void Net_TransportClose(void) {
}

static uint8_t Net_EnsureInitialized(void) {
    if (net_initialized == 0U) {
        Net_Init();
    }

    return (uint8_t)(net_last_error == NET_STATUS_OK);
}

static char *Net_JsonEscape(const char *content, uint16_t size) {
    size_t escaped_size = 0U;
    char *escaped = NULL;
    char *write = NULL;

    if (content == NULL) {
        return NULL;
    }

    if (size == 0U) {
        size = (uint16_t)strlen(content);
    }

    for (uint16_t i = 0U; i < size; i++) {
        const unsigned char ch = (unsigned char)content[i];

        switch (ch) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                escaped_size += 2U;
                break;
            default:
                escaped_size += (ch < 0x20U) ? 6U : 1U;
                break;
        }
    }

    escaped = (char *)malloc(escaped_size + 1U);
    if (escaped == NULL) {
        return NULL;
    }

    write = escaped;
    for (uint16_t i = 0U; i < size; i++) {
        const unsigned char ch = (unsigned char)content[i];

        switch (ch) {
            case '"':
                *write++ = '\\';
                *write++ = '"';
                break;
            case '\\':
                *write++ = '\\';
                *write++ = '\\';
                break;
            case '\b':
                *write++ = '\\';
                *write++ = 'b';
                break;
            case '\f':
                *write++ = '\\';
                *write++ = 'f';
                break;
            case '\n':
                *write++ = '\\';
                *write++ = 'n';
                break;
            case '\r':
                *write++ = '\\';
                *write++ = 'r';
                break;
            case '\t':
                *write++ = '\\';
                *write++ = 't';
                break;
            default:
                if (ch < 0x20U) {
                    static const char hex[] = "0123456789abcdef";

                    *write++ = '\\';
                    *write++ = 'u';
                    *write++ = '0';
                    *write++ = '0';
                    *write++ = hex[(ch >> 4U) & 0x0FU];
                    *write++ = hex[ch & 0x0FU];
                } else {
                    *write++ = (char)ch;
                }
                break;
        }
    }
    *write = '\0';

    return escaped;
}

static uint8_t Net_ParseUrl(const char *url, Net_EndpointType *endpoint) {
    const char *cursor = url;
    const char *host_start = NULL;
    const char *host_end = NULL;
    const char *path_start = NULL;
    const char *port_start = NULL;
    size_t host_len = 0U;
    size_t path_len = 0U;

    if ((url == NULL) || (endpoint == NULL)) {
        return 0U;
    }

    endpoint->use_tls = 0U;
    endpoint->port = 80U;

    if (strncmp(cursor, "https://", 8U) == 0) {
        endpoint->use_tls = 1U;
        endpoint->port = 443U;
        cursor += 8U;
    } else if (strncmp(cursor, "http://", 7U) == 0) {
        cursor += 7U;
    }

    host_start = cursor;
    path_start = strchr(host_start, '/');
    host_end = (path_start == NULL) ? (host_start + strlen(host_start)) : path_start;

    for (const char *scan = host_start; scan < host_end; scan++) {
        if (*scan == ':') {
            port_start = scan;
            break;
        }
    }

    host_len = (port_start == NULL) ? (size_t)(host_end - host_start) : (size_t)(port_start - host_start);
    if ((host_len == 0U) || (host_len >= NET_HOST_MAX_SIZE)) {
        return 0U;
    }

    memcpy(endpoint->host, host_start, host_len);
    endpoint->host[host_len] = '\0';

    if (port_start != NULL) {
        unsigned long port = 0UL;

        for (const char *scan = port_start + 1; scan < host_end; scan++) {
            if (isdigit((unsigned char)*scan) == 0) {
                return 0U;
            }
            port = (port * 10UL) + (unsigned long)(*scan - '0');
            if (port > 65535UL) {
                return 0U;
            }
        }

        if (port == 0UL) {
            return 0U;
        }
        endpoint->port = (uint16_t)port;
    }

    if (path_start == NULL) {
        endpoint->path[0] = '/';
        endpoint->path[1] = '\0';
    } else {
        path_len = strlen(path_start);
        if ((path_len == 0U) || (path_len >= NET_PATH_MAX_SIZE)) {
            return 0U;
        }
        memcpy(endpoint->path, path_start, path_len + 1U);
    }

    return 1U;
}

static uint8_t Net_CaseContains(const char *data, size_t data_size, const char *needle) {
    const size_t needle_size = strlen(needle);

    if ((data == NULL) || (needle == NULL) || (needle_size == 0U) || (needle_size > data_size)) {
        return 0U;
    }

    for (size_t i = 0U; i <= (data_size - needle_size); i++) {
        size_t j = 0U;

        for (; j < needle_size; j++) {
            const int left = tolower((unsigned char)data[i + j]);
            const int right = tolower((unsigned char)needle[j]);

            if (left != right) {
                break;
            }
        }

        if (j == needle_size) {
            return 1U;
        }
    }

    return 0U;
}

static uint8_t Net_FindHttpBody(char *raw, size_t raw_size, size_t *body_offset) {
    if ((raw == NULL) || (body_offset == NULL)) {
        return 0U;
    }

    for (size_t i = 0U; (i + 3U) < raw_size; i++) {
        if ((raw[i] == '\r') && (raw[i + 1U] == '\n') && (raw[i + 2U] == '\r') && (raw[i + 3U] == '\n')) {
            *body_offset = i + 4U;
            return 1U;
        }
    }

    for (size_t i = 0U; (i + 1U) < raw_size; i++) {
        if ((raw[i] == '\n') && (raw[i + 1U] == '\n')) {
            *body_offset = i + 2U;
            return 1U;
        }
    }

    *body_offset = 0U;
    return 0U;
}

static uint8_t Net_IsHex(char ch) {
    return (uint8_t)(((ch >= '0') && (ch <= '9')) ||
                     ((ch >= 'a') && (ch <= 'f')) ||
                     ((ch >= 'A') && (ch <= 'F')));
}

static uint8_t Net_HexValue(char ch) {
    if ((ch >= '0') && (ch <= '9')) {
        return (uint8_t)(ch - '0');
    }
    if ((ch >= 'a') && (ch <= 'f')) {
        return (uint8_t)(ch - 'a' + 10);
    }
    return (uint8_t)(ch - 'A' + 10);
}

static size_t Net_DecodeChunkedBody(char *body, size_t body_size) {
    char *read = body;
    char *write = body;
    char *end = body + body_size;

    while (read < end) {
        unsigned long chunk_size = 0UL;
        uint8_t has_digit = 0U;

        while ((read < end) && ((*read == '\r') || (*read == '\n'))) {
            read++;
        }

        while (read < end) {
            const char ch = *read++;

            if (ch == ';') {
                while ((read < end) && (*read != '\n')) {
                    read++;
                }
                if ((read < end) && (*read == '\n')) {
                    read++;
                }
                break;
            }

            if (ch == '\r') {
                if ((read < end) && (*read == '\n')) {
                    read++;
                }
                break;
            }

            if (ch == '\n') {
                break;
            }

            if (Net_IsHex(ch) == 0U) {
                return (size_t)(write - body);
            }

            has_digit = 1U;
            chunk_size = (chunk_size * 16UL) + (unsigned long)Net_HexValue(ch);
        }

        if ((has_digit == 0U) || (chunk_size == 0UL)) {
            break;
        }

        if ((size_t)(end - read) < (size_t)chunk_size) {
            break;
        }

        memmove(write, read, (size_t)chunk_size);
        write += chunk_size;
        read += chunk_size;

        if ((read < end) && (*read == '\r')) {
            read++;
        }
        if ((read < end) && (*read == '\n')) {
            read++;
        }
    }

    *write = '\0';
    return (size_t)(write - body);
}

static char *Net_BuildHttpRequest(const Net_SendMessageType *message,
                                  const Net_EndpointType *endpoint,
                                  size_t *request_size) {
    const char *header1 = message->header1;
    const char *header2 = message->header2;
    const char *request_format_with_auth =
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "%s\r\n"
        "%s\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    const char *request_format =
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "%s\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    size_t body_size = 0U;
    size_t buffer_size = 0U;
    char *request = NULL;
    int written = 0;

    if ((message == NULL) || (endpoint == NULL) || (request_size == NULL) || (message->data == NULL)) {
        return NULL;
    }

    if (header1 == NULL) {
        header1 = NET_CONTENT_TYPE_HEADER;
    }
    if (header2 == NULL) {
        header2 = "";
    }

    body_size = strlen(message->data);
    buffer_size = strlen(request_format_with_auth) + strlen(endpoint->path) + strlen(endpoint->host) +
                  strlen(header1) + strlen(header2) + body_size + 32U;
    request = (char *)malloc(buffer_size);
    if (request == NULL) {
        return NULL;
    }

    if (header2[0] != '\0') {
        written = snprintf(request, buffer_size, request_format_with_auth,
                           endpoint->path,
                           endpoint->host,
                           header1,
                           header2,
                           (unsigned long)body_size,
                           message->data);
    } else {
        written = snprintf(request, buffer_size, request_format,
                           endpoint->path,
                           endpoint->host,
                           header1,
                           (unsigned long)body_size,
                           message->data);
    }

    if ((written < 0) || ((size_t)written >= buffer_size)) {
        free(request);
        return NULL;
    }

    *request_size = (size_t)written;
    return request;
}

void Net_MessageBuildMessage(Net_SendMessageType *message,char *api_key,char *data) {
    const size_t header_size = strlen(NET_AUTH_HEADER_PREFIX) + ((api_key == NULL) ? 0U : strlen(api_key)) + 1U;

    if (message == NULL) {
        Net_SetLastError(NET_STATUS_INVALID_ARGUMENT);
        return;
    }

    message->curl = NET_API_URL;
    message->header1 = NET_CONTENT_TYPE_HEADER;
    message->header2 = (char *)malloc(header_size);
    message->data = data;

    if (message->header2 == NULL) {
        Net_SetLastError(NET_STATUS_NO_MEMORY);
        return;
    }

    snprintf(message->header2, header_size, "%s%s", NET_AUTH_HEADER_PREFIX, (api_key == NULL) ? "" : api_key);
    Net_SetLastError(NET_STATUS_OK);
}

void Net_MessageFree(Net_SendMessageType *message) {
    if (message == NULL) {
        return;
    }

    free(message->header2);
    free(message->data);

    message->curl = NULL;
    message->header1 = NULL;
    message->header2 = NULL;
    message->data = NULL;
}

void Net_ResponseFree(Net_ResponseType *response) {
    if (response == NULL) {
        return;
    }

    free(response->data);
    response->data = NULL;
    response->size = 0U;
}

char* Net_MessageBuildData(char *content,uint16_t size) {
    static const char system_prompt[] =
        "You are a helpful assistant,you need to return message like "
        "\"time:xxx;happened:yyy;EOF;\",the key words you can use only include "
        "\"time\" and \"happened\",end an event with \"EOF;\".Don't use \\n,"
        "represent BC with \"-\" in the head of number.if you search unsuccessfully,"
        "just return \"EOF;\" directly";
    static const char data_format[] =
        "{\"model\":\"deepseek-v4-flash\",\"messages\":["
        "{\"role\":\"system\",\"content\":\"%s\"},"
        "{\"role\":\"user\",\"content\":\"%s\"}],"
        "\"thinking\":{\"type\":\"disabled\"},\"stream\":false}";
    char *escaped_system = NULL;
    char *escaped_content = NULL;
    char *data = NULL;
    size_t data_size = 0U;

    if (content == NULL) {
        Net_SetLastError(NET_STATUS_INVALID_ARGUMENT);
        return NULL;
    }

    escaped_system = Net_JsonEscape(system_prompt, (uint16_t)strlen(system_prompt));
    escaped_content = Net_JsonEscape(content, size);
    if ((escaped_system == NULL) || (escaped_content == NULL)) {
        free(escaped_system);
        free(escaped_content);
        Net_SetLastError(NET_STATUS_NO_MEMORY);
        return NULL;
    }

    data_size = strlen(data_format) + strlen(escaped_system) + strlen(escaped_content) + 1U;
    data = (char *)malloc(data_size);
    if (data == NULL) {
        free(escaped_system);
        free(escaped_content);
        Net_SetLastError(NET_STATUS_NO_MEMORY);
        return NULL;
    }

    snprintf(data, data_size, data_format, escaped_system, escaped_content);

    free(escaped_system);
    free(escaped_content);
    Net_SetLastError(NET_STATUS_OK);

    return data;
}

void Net_SendMessage(const Net_SendMessageType *message) {
    Net_EndpointType endpoint;
    char *request = NULL;
    size_t request_size = 0U;
    size_t sent_size = 0U;

    if ((message == NULL) || (message->curl == NULL) || (message->data == NULL)) {
        Net_SetLastError(NET_STATUS_INVALID_ARGUMENT);
        return;
    }

    if (Net_EnsureInitialized() == 0U) {
        return;
    }

    if (Net_ParseUrl(message->curl, &endpoint) == 0U) {
        Net_SetLastError(NET_STATUS_BAD_URL);
        return;
    }

    request = Net_BuildHttpRequest(message, &endpoint, &request_size);
    if (request == NULL) {
        Net_SetLastError(NET_STATUS_NO_MEMORY);
        return;
    }

    if (Net_TransportOpen(endpoint.host, endpoint.port, endpoint.use_tls) != 0) {
        free(request);
        Net_SetLastError(NET_STATUS_TRANSPORT_ERROR);
        return;
    }

    while (sent_size < request_size) {
        const size_t left_size = request_size - sent_size;
        const uint16_t chunk_size = (left_size > 65535U) ? 65535U : (uint16_t)left_size;
        const int current_sent = Net_TransportSend(request + sent_size, chunk_size);

        if ((current_sent <= 0) || (current_sent > (int)chunk_size)) {
            free(request);
            Net_TransportClose();
            Net_SetLastError(NET_STATUS_TRANSPORT_ERROR);
            return;
        }

        sent_size += (size_t)current_sent;
    }

    free(request);
    Net_SetLastError(NET_STATUS_OK);
}

void Net_ReceiveMessage(Net_ResponseType *response) {
    char *raw = NULL;
    size_t raw_capacity = NET_HTTP_RX_CHUNK_SIZE;
    size_t raw_size = 0U;
    size_t body_offset = 0U;
    size_t body_size = 0U;
    uint8_t has_http_header = 0U;

    if (response == NULL) {
        Net_SetLastError(NET_STATUS_INVALID_ARGUMENT);
        return;
    }

    response->size = 0U;
    response->data = NULL;

    if (Net_EnsureInitialized() == 0U) {
        return;
    }

    raw = (char *)malloc(raw_capacity + 1U);
    if (raw == NULL) {
        Net_SetLastError(NET_STATUS_NO_MEMORY);
        return;
    }

    while (raw_size < NET_HTTP_RESPONSE_MAX_SIZE) {
        char chunk[NET_HTTP_RX_CHUNK_SIZE];
        const int received = Net_TransportReceive(chunk, (uint16_t)sizeof(chunk), NET_HTTP_TIMEOUT_MS);

        if (received < 0) {
            free(raw);
            Net_TransportClose();
            Net_SetLastError(NET_STATUS_TRANSPORT_ERROR);
            return;
        }

        if (received == 0) {
            break;
        }

        if (received > (int)sizeof(chunk)) {
            free(raw);
            Net_TransportClose();
            Net_SetLastError(NET_STATUS_TRANSPORT_ERROR);
            return;
        }

        if ((raw_size + (size_t)received) > NET_HTTP_RESPONSE_MAX_SIZE) {
            free(raw);
            Net_TransportClose();
            Net_SetLastError(NET_STATUS_RESPONSE_TOO_LARGE);
            return;
        }

        if ((raw_size + (size_t)received + 1U) > raw_capacity) {
            size_t new_capacity = raw_capacity * 2U;
            char *new_raw = NULL;

            while (new_capacity < (raw_size + (size_t)received + 1U)) {
                new_capacity *= 2U;
            }
            if (new_capacity > NET_HTTP_RESPONSE_MAX_SIZE) {
                new_capacity = NET_HTTP_RESPONSE_MAX_SIZE;
            }

            new_raw = (char *)realloc(raw, new_capacity + 1U);
            if (new_raw == NULL) {
                free(raw);
                Net_TransportClose();
                Net_SetLastError(NET_STATUS_NO_MEMORY);
                return;
            }

            raw = new_raw;
            raw_capacity = new_capacity;
        }

        memcpy(raw + raw_size, chunk, (size_t)received);
        raw_size += (size_t)received;
    }
    raw[raw_size] = '\0';

    Net_TransportClose();

    has_http_header = Net_FindHttpBody(raw, raw_size, &body_offset);
    body_size = raw_size - body_offset;

    if ((has_http_header != 0U) &&
        (Net_CaseContains(raw, body_offset, "transfer-encoding:") != 0U) &&
        (Net_CaseContains(raw, body_offset, "chunked") != 0U)) {
        body_size = Net_DecodeChunkedBody(raw + body_offset, body_size);
    }

    if (body_size > 65535U) {
        free(raw);
        Net_SetLastError(NET_STATUS_RESPONSE_TOO_LARGE);
        return;
    }

    response->data = (char *)malloc(body_size + 1U);
    if (response->data == NULL) {
        free(raw);
        Net_SetLastError(NET_STATUS_NO_MEMORY);
        return;
    }

    memcpy(response->data, raw + body_offset, body_size);
    response->data[body_size] = '\0';
    response->size = (uint16_t)body_size;

    free(raw);
    Net_SetLastError(NET_STATUS_OK);
}

void Net_Init(void) {
    if (Net_TransportInit() == 0) {
        net_initialized = 1U;
        Net_SetLastError(NET_STATUS_OK);
    } else {
        net_initialized = 0U;
        Net_SetLastError(NET_STATUS_TRANSPORT_ERROR);
    }
}

int16_t Net_GetLastError(void) {
    return net_last_error;
}
