#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "connection.h"
#include "http.h"
#include "server.h"

static int freeConnectionsCount = 0;
static connection *freeConnections = NULL;

connection *connCreate(int fd) {
    connection *conn;

    if (freeConnectionsCount > 0) {
        conn = freeConnections;
        freeConnections = conn->next;
        freeConnectionsCount--;
    } else {
        conn = malloc(sizeof(connection) + CONN_SEND_BUFFER_SIZE + CONN_RECV_BUFFER_SIZE);
        if (conn == NULL) return NULL;

        /* 将 sendBuffer 指向 conn 的末尾 */
        conn->sendBuffer = (char *)conn + sizeof(*conn);
        conn->sendEnd = conn->sendBuffer + CONN_SEND_BUFFER_SIZE;
        /* 将 recvBuffer 指向 sendEnd 的地址 */
        conn->recvBuffer = conn->sendEnd;
        conn->recvEnd = conn->recvBuffer + CONN_RECV_BUFFER_SIZE;
    }

    conn->fd = fd;
    conn->sendBytes = 0;
    conn->recvBytes = 0;
    conn->currentPackageLen = 0;
    conn->status = CONN_ESTABLISHED;

    /* 初始化缓冲区 */
    conn->sendPos = conn->sendBuffer;
    conn->sendLast = conn->sendBuffer;
    conn->recvPos = conn->recvBuffer;
    conn->recvLast = conn->recvBuffer;

    fcntl(fd, F_SETFL, O_NONBLOCK);

    return conn;
}

void connSend(connection *conn, char *buffer, int raw) {
    int len, bufferLen;

    if (conn->status == CONN_CLOSING || conn->status == CONN_CLOSED) {
        return;
    }

    if (raw == CONN_SEND_NOT_RAW) {
        /* TODO: http encode */
        buffer = NULL;
    }

    bufferLen = strlen(buffer);
    if (bufferLen == 0) {
        return;
    } else if (bufferLen > (conn->sendEnd - conn->sendLast)) {
        printf("fd: %d, buffer too big\n", conn->fd);
        return;
    }

    memcpy(conn->sendLast, buffer, bufferLen);
    conn->sendLast += bufferLen;

    len = write(conn->fd, conn->sendPos, (conn->sendLast - conn->sendPos));
    if (len <= 0) {
        /* 发送失败，客户端可能已关闭 */
        if (len == 0 || (errno != EINTR && errno != EAGAIN))
            connDestroy(conn);
        return;
    }

    conn->sendPos += len;

    /* sendPos >= sendLast 说明数据全部被发出去了 */
    if (conn->sendPos >= conn->sendLast) {
        conn->sendPos = conn->sendBuffer;
        conn->sendLast = conn->sendBuffer;
        return;
    }

    /* 添加可写事件 */
    aeCreateFileEvent(server.el, conn->fd, AE_WRITABLE, connWrite, conn);
}

void connRead(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    int ret, remaining;
    char buf[CONN_READ_BUFFER_SIZE];
    connection *conn = (connection *) clientData;

    remaining = conn->recvEnd - conn->recvLast;
    if (remaining <= 0) {
        printf("fd: %d, header too big\n", conn->fd);
        connClose(conn, "HTTP/1.1 413 Request Entity Too Large\r\n\r\n", CONN_SEND_RAW);
        return;
    }

    ret = read(fd, conn->recvLast, remaining);
    if (ret <= 0) {
        /* 发送失败，客户端可能已关闭 */
        if (ret == 0 || (errno != EINTR && errno != EAGAIN))
            connDestroy(conn);
        return;
    }

    conn->recvLast += ret;
    printf("[%d]recved:%d\n", conn->fd, ret);

    conn->currentPackageLen = httpCheck(conn, conn->recvPos);
    if (conn->currentPackageLen == 0) {
        return;
    }

    conn->onMessage(conn, conn->recvPos);
}

void connWrite(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    int len;
    connection *conn = (connection *) clientData;

    len = write(fd, conn->sendPos, (conn->sendLast - conn->sendPos));
    if (len <= 0) {
        /* 发送失败，客户端可能已关闭 */
        if (len == 0 || (errno != EINTR && errno != EAGAIN))
            connDestroy(conn);
        return;
    }

    conn->sendPos += len;

    if (conn->sendPos < conn->sendLast) {
        /* 还有数据，等会继续发送 */
        return;
    }

    conn->sendPos = conn->sendBuffer;
    conn->sendLast = conn->sendBuffer;
    /* 数据全部被发送出去，删除可写事件。 */
    aeDeleteFileEvent(server.el, fd, AE_WRITABLE);
}

void connClose(connection *conn, char *data, int raw) {
    if (conn->status == CONN_ESTABLISHED) {
        connDestroy(conn);
        return;
    } else if (conn->status == CONN_CLOSING || conn->status == CONN_CLOSED) {
        return;
    }

    if (strlen(data) > 0) {
        connSend(conn, data, raw);
    }

    conn->status = CONN_CLOSING;

    if (strlen(conn->sendBuffer) == 0) {
        connDestroy(conn);
        return;
    }

    /* 删除可读事件 */
    aeDeleteFileEvent(server.el, conn->fd, AE_READABLE);
}

void connDestroy(connection *conn) {
    /* 删除可读、可写事件 */
    aeDeleteFileEvent(server.el, conn->fd, (AE_READABLE|AE_WRITABLE));

    close(conn->fd);

    if (freeConnectionsCount >= FREE_CONNECTIONS_MAX_SIZE) {
        free(conn);
        return;
    }

    conn->status = CONN_CLOSED;
    conn->next = freeConnections;
    freeConnections = conn;
    freeConnectionsCount++;
}
