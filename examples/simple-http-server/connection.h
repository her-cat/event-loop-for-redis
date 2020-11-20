#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "../../src/ae/ae.h"

#define CONN_READ_BUFFER_SIZE 4096

typedef struct connection connection;

struct connection {
    int fd;
    int recvBytes;
    char recvBuffer[1024];
    void (*onMessage)(aeEventLoop *eventLoop, int fd, connection *conn);
};

connection *connCreate(int fd);
void connRead(aeEventLoop *eventLoop, int fd, void *clientData, int mask);
void connDestroy(connection * conn);

#endif
