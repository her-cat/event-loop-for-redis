#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "../../src/ae/ae.h"

#define CONN_SEND_RAW 1
#define CONN_SEND_NOT_RAW 0
#define CONN_READ_BUFFER_SIZE 1024
#define CONN_SEND_BUFFER_SIZE 4096
#define CONN_RECV_BUFFER_SIZE CONN_SEND_BUFFER_SIZE

enum CONN_STATUS {
    CONN_INITIAL = 0,
    CONN_CONNECTING,
    CONN_ESTABLISHED,
    CONN_CLOSING,
    CONN_CLOSED
};

typedef struct connection connection;

struct connection {
    int fd;
    int status;
    int recvBytes;
    int sendBytes;
    char *sendBuffer;
    char *recvBuffer;
    int currentPackageLen;
    char *clientAddr;
    int clientPort;
    void (*onMessage)(connection *conn, char *buffer);
};

connection *connCreate(int fd);
void connSend(connection *conn, char *buffer, int raw);
void connRead(aeEventLoop *eventLoop, int fd, void *clientData, int mask);
void connWrite(aeEventLoop *eventLoop, int fd, void *clientData, int mask);
void connClose(connection *conn, char *data, int raw);
void connDestroy(connection *conn);

#endif
