#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "request.h"
#include "until.h"

request *reqCreate(connection *conn, char *buffer) {
    request *req = malloc(sizeof(request));
    if (req == NULL) return NULL;

    req->conn = conn;
    req->buffer = buffer;

    reqParseFirstLine(req, buffer);

    return req;
}

int reqParseFirstLine(request *req, char *buffer) {
    int pos = 0;
    char *p = buffer;
    pos = strpos(p, " ");
    strncpy(req->method, p, pos);

    p += pos;
    pos = strpos(p, " ");

    printf("req->method: %s\n", req->method);

    return 1;
}
