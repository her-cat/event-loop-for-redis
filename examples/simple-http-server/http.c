#include <string.h>
#include <stdio.h>
#include "http.h"
#include "until.h"

int httpCheck(connection *conn, char *buffer) {
    char method[8], *pos, *newline;
    int crlfPos, headerLen, firstSpacePos, bodyLen, ok;

    /* 没有 \r\n\r\n 说明 header 不完整。 */
    if ((crlfPos = strpos(buffer, HTTP_CRLF_CRLF)) < 0) {
        printf("need more header data\n");
        return 0;
    }

    headerLen = crlfPos + 4;
    firstSpacePos = strpos(buffer, " ");
    if (firstSpacePos <= 0) {
        connClose(conn, "HTTP/1.1 400 Bad Request\r\n\r\n", CONN_SEND_RAW);
        return 0;
    }

    strncpy(method, buffer, firstSpacePos);

    if (strncasecmp("GET", method, 3) == 0 ||
        strncasecmp("HEAD", method, 4) == 0 ||
        strncasecmp("DELETE", method, 6) == 0 ||
        strncasecmp("OPTIONS", method, 7) == 0) {
        /* 不需要解析 body 的请求方式 */
        return headerLen;
    } else if (strncasecmp("POST", method, 4) != 0 && strncasecmp("PUT", method, 3) != 0) {
        /* 未知的请求方式 */
        connClose(conn, "HTTP/1.1 400 Bad Request\r\n\r\n", CONN_SEND_RAW);
        return 0;
    }

    pos = strstr(buffer, "Content-Length: ");
    newline = strchr(pos, '\r');

    if (pos != NULL && newline != NULL) {
        pos += 16;
        if (str2int(pos, newline - pos, &bodyLen)) {
            return headerLen + bodyLen;
        }
    }
    /* Body 长度获取失败。*/
    connClose(conn, "HTTP/1.1 400 Bad Request\r\n\r\n", CONN_SEND_RAW);
    return 0;
}
