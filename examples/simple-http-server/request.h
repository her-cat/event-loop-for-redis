#ifndef __REQUEST_H__
#define __REQUEST_H__

#include "connection.h"

typedef struct request {
    connection *conn;
    char *buffer;

    /* first line properties. */
    char *uri;
    char *path;
    char *method;
    float protocol_version;

    /* use double linked list to store headers. */
} request;

request *reqCreate(connection *conn, char *buffer);

#endif
