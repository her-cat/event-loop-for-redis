#ifndef __HTTP_H__
#define __HTTP_H__

#include "connection.h"

#define HTTP_CRLF_CRLF "\r\n\r\n"

int httpCheck(connection *conn, char *buffer);
int httpGetContentLength(char *buffer, int headerLen, int *length);

#endif
