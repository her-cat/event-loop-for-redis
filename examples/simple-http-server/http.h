#ifndef __HTTP_H__
#define __HTTP_H__

#include "connection.h"
#include "request.h"

#define HTTP_CRLF_CRLF "\r\n\r\n"
#define HTTP_MAX_PACKAGE_LENGTH 4096

int httpCheck(connection *conn, char *buffer);
int httpParseFirstLine(request *req, char *buffer);
int httpGetContentLength(char *buffer, int headerLen, int *length);

#endif
