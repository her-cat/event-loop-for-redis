#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "connection.h"
#include "until.h"

connection *connCreate(int fd) {
    connection *conn = malloc(sizeof(connection));
    if (conn == NULL) return NULL;

    conn->fd = fd;
    conn->recvBytes = 0;
    bzero(conn->recvBuffer, sizeof(conn->recvBuffer));

    return conn;
}

void connRead(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    int ret;
    char buf[CONN_READ_BUFFER_SIZE];
    connection *conn = (connection *) clientData;

    ret = read(fd, buf, CONN_READ_BUFFER_SIZE);
    if (ret < 0) {
        connDestroy(conn);
        aeDeleteFileEvent(eventLoop, fd, (AE_READABLE|AE_WRITABLE));
        return;
    } else if (ret == EINTR || ret == EAGAIN) {
        return;
    }

    printf("recved:%d\n", ret);
    conn->recvBytes += ret;
    strncat(conn->recvBuffer, buf, ret);

    if (strpos(conn->recvBuffer, "\r\n\r\n") < 0) {
        printf("need more data\n");
        return;
    }

    /* TODO: 解析body */

    conn->onMessage(eventLoop, fd, conn);
}

void connDestroy(connection * conn) {
    close(conn->fd);
    free(conn->recvBuffer);
    free(conn);
}
