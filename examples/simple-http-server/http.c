#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "http.h"
#include "until.h"

int httpCheck(connection *conn, char *buffer) {
    int crlfPos, headerLen, bodyLen;

    /* 没有 \r\n\r\n 说明 header 不完整。 */
    if ((crlfPos = strpos(buffer, HTTP_CRLF_CRLF)) < 0) {
        /* 判断包长度是否超出限制。 */
        if (strlen(buffer) >= HTTP_MAX_PACKAGE_LENGTH) {
            connClose(conn, "HTTP/1.1 413 Request Entity Too Large\r\n\r\n", CONN_SEND_RAW);
        }
        printf("need more header data\n");
        return 0;
    }

    headerLen = crlfPos + 4;

    if (strncasecmp("GET", buffer, 3) == 0 ||
        strncasecmp("HEAD", buffer, 4) == 0 ||
        strncasecmp("DELETE", buffer, 6) == 0 ||
        strncasecmp("OPTIONS", buffer, 7) == 0) {
        /* 不需要解析 body 的请求方式 */
        return headerLen;
    } else if (strncasecmp("POST", buffer, 4) != 0 && strncasecmp("PUT", buffer, 3) != 0) {
        /* 未知的请求方式 */
        connClose(conn, "HTTP/1.1 400 Bad Request\r\n\r\n", CONN_SEND_RAW);
        return 0;
    }

    if (httpGetContentLength(buffer, headerLen, &bodyLen)) {
        return headerLen + bodyLen;
    }

    connClose(conn, "HTTP/1.1 400 Bad Request\r\n\r\n", CONN_SEND_RAW);
    return 0;
}

int httpGetContentLength(char *buffer, int headerLen, int *length) {
    int ret;
    char *pos, *newline, *header;

    header = malloc(sizeof(char) * headerLen);
    if (header == NULL) {
        return 0;
    }

    strncpy(header, buffer, headerLen);
    if ((pos = strstr(header, "\r\nContent-Length: ")) == NULL) {
        free(header);
        return 0;
    }

    pos += 18;
    if ((newline = strchr(pos, '\r')) == NULL) {
        free(header);
        return 0;
    }

    ret = str2int(pos, newline - pos, length);

    free(header);
    return ret;
}
