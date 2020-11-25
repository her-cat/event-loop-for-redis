#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "connection.h"
#include "http.h"
#include "server.h"

connection *connCreate(int fd) {
    connection *conn = malloc(sizeof(connection));
    if (conn == NULL) return NULL;

    conn->sendBuffer = malloc(sizeof(char) * CONN_SEND_BUFFER_SIZE);
    conn->recvBuffer = malloc(sizeof(char) * CONN_RECV_BUFFER_SIZE);
    if (conn->sendBuffer == NULL || conn->recvBuffer == NULL) {
        free(conn->sendBuffer);
        free(conn->recvBuffer);
        free(conn);
        return NULL;
    }

    conn->fd = fd;
    conn->sendBytes = 0;
    conn->recvBytes = 0;
    conn->currentPackageLen = 0;
    conn->status = CONN_ESTABLISHED;
    memset(conn->sendBuffer, 0, strlen(conn->sendBuffer));
    memset(conn->recvBuffer, 0, strlen(conn->recvBuffer));

    fcntl(fd, F_SETFL, O_NONBLOCK);

    return conn;
}

void connSend(connection *conn, char *buffer, int raw) {
    int len;

    if (conn->status == CONN_CLOSING || conn->status == CONN_CLOSED) {
        return;
    }

    if (raw == CONN_SEND_NOT_RAW) {
        /* TODO: http encode */
        buffer = NULL;
    }

    if (strlen(buffer) == 0) {
        return;
    }

    len = write(conn->fd, buffer, strlen(buffer));
    if (len == strlen(buffer)) {
        /* 数据全部发出去了。 */
        conn->sendBytes += len;
        memset(conn->sendBuffer, 0, strlen(conn->sendBuffer));
        return;
    } else if (len > 0) {
        /* 只发出去了部分数据。 */
        strcpy(conn->sendBuffer, buffer + len);
        conn->sendBytes += len;
    } else {
        /* 发送失败，客户端可能已关闭 */
        if (len == 0 || errno == EPIPE) {
            connDestroy(conn);
            return;
        }
        strcpy(conn->sendBuffer, buffer);
    }
    /* 添加可写事件 */
    aeCreateFileEvent(server.el, conn->fd, AE_WRITABLE, connWrite, conn);
}

void connRead(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    int ret;
    char buf[CONN_READ_BUFFER_SIZE];
    connection *conn = (connection *) clientData;

    ret = read(fd, buf, CONN_READ_BUFFER_SIZE);
    if (ret <= 0) {
        /* EINTR: 表示操作被中断，可以继续读取。
         * EAGAIN/EWOULDBLOCK: 表示接收缓冲区现在没有数据，过会再重试。 */
        if (errno == EINTR || errno == EAGAIN)
            return;
        /* 发生错误，客户端可能已关闭。 */
        connDestroy(conn);
        return;
    }

    conn->recvBytes += ret;
    strncat(conn->recvBuffer, buf, ret);
    printf("[%d]recved:%d\n", conn->fd, ret);

    while (strlen(conn->recvBuffer) > 0) {
        if (conn->currentPackageLen > 0) {
            if (conn->currentPackageLen > strlen(conn->recvBuffer)) {
                break;
            }
        } else {
            conn->currentPackageLen = httpCheck(conn, conn->recvBuffer);
            if (conn->currentPackageLen == 0) {
                break;
            } else if (conn->currentPackageLen > 0 && conn->currentPackageLen <= 104857) {
                if (conn->currentPackageLen > strlen(conn->recvBuffer)) {
                    break;
                }
            } else {
                /* 错误的数据包。 */
                printf("Error package\n");
                connDestroy(conn);
                return;
            }
        }

        /* 从缓冲区中获取完整的包。 */
        char *oneRequestBuffer = malloc(sizeof(char) * conn->currentPackageLen);
        strncpy(oneRequestBuffer, conn->recvBuffer, conn->currentPackageLen);
        /* 如果包的长度等于缓冲区的长度，说明是一个完整的包，直接清空 recvBuffer。
         * 否则从 recvBuffer 中移除当前包的数据。 */
        if (conn->currentPackageLen == strlen(conn->recvBuffer)) {
            memset(conn->recvBuffer, 0, strlen(conn->recvBuffer));
        } else {
            strcpy(conn->recvBuffer, conn->recvBuffer + conn->currentPackageLen);
        }
        /* 重置当前包长度为 0。*/
        conn->currentPackageLen = 0;
        if (conn->onMessage == NULL)
            continue;;
        conn->onMessage(conn, oneRequestBuffer);
    }
}

void connWrite(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    int len;
    connection *conn = (connection *) clientData;

    len = write(fd, conn->sendBuffer, strlen(conn->sendBuffer));
    if (len == strlen(conn->sendBuffer)) {
        conn->sendBytes += len;
        /* 数据全部被发送出去，删除可写事件。 */
        aeDeleteFileEvent(server.el, fd, AE_WRITABLE);
        memset(conn->sendBuffer, 0, strlen(conn->sendBuffer));
        /* TODO: 通知 sendBuffer 有空闲 */
        if (conn->status == CONN_CLOSING) {
            connDestroy(conn);
        }
        return;
    } else if (len > 0) {
        /* 只发送了部分数据。 */
        conn->sendBytes += len;
        strcpy(conn->sendBuffer, conn->sendBuffer + len);
        return;
    } else if (errno == EINTR || errno == EAGAIN) {
        /* EINTR: 表示操作被中断，可以继续发送。
         * EAGAIN/EWOULDBLOCK: 表示发送缓冲区没有空间，过会再重试。 */
        return;
    }
    /* 发送失败，客户端可能已关闭。 */
    connDestroy(conn);
}

void connClose(connection *conn, char *data, int raw) {
    if (conn->status == CONN_CONNECTING) {
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
    free(conn);
}
