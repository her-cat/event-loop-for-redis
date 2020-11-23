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

    conn->fd = fd;
    conn->sendBytes = 0;
    conn->recvBytes = 0;
    conn->sendBytes = 0;
    conn->currentPackageLen = 0;
	conn->status = CONN_ESTABLISHED;
    memset(conn->sendBuffer, 0, sizeof(conn->sendBuffer));
    memset(conn->recvBuffer, 0, sizeof(conn->recvBuffer));

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
		/* 数据全部发出去了 */
		conn->sendBytes += len;
		return;
	} else if (len > 0) {
		/* 只发出去了部分数据 */
		strncpy(conn->sendBuffer, buffer, len);
		conn->sendBytes += len;
	} else {
		/* 发送失败（客户端已关闭？） */
		if (errno == EPIPE) {
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
    if (ret < 0) {
		connDestroy(conn);
        return;
    } else if (ret == EINTR || ret == EAGAIN) {
        return;
    }

    conn->recvBytes += ret;
    strncat(conn->recvBuffer, buf, ret);
	printf("[%d]recved:%d\n", conn->fd, ret);

	if (strlen(conn->recvBuffer) == 0) {
		return;
	}

	conn->currentPackageLen = httpCheck(conn, conn->recvBuffer);
	if (conn->currentPackageLen <= 0) {
		return;
	}

	if (conn->onMessage == NULL) {
		memset(conn->recvBuffer, 0, sizeof(conn->recvBuffer));
		return;
	}

    conn->onMessage(conn);
}

void connWrite(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
	int len;
	connection *conn = (connection *) clientData;

	len = write(fd, conn->sendBuffer, strlen(conn->sendBuffer));
	if (len == strlen(conn->sendBuffer)) {
		conn->sendBytes += len;
		/* 全部数据发完了，删除可写事件 */
		aeDeleteFileEvent(server.el, fd, AE_WRITABLE);
		memset(conn->sendBuffer, 0, sizeof(conn->sendBuffer));
		/* TODO: 通知 sendBuffer 有空闲 */
		if (conn->status == CONN_CLOSING) {
			connDestroy(conn);
		}
		return;
	} else if (len > 0) {
		/* 发送了部分数据 */
		conn->sendBytes += len;
		strncpy(conn->sendBuffer, conn->sendBuffer, len);
		return;
	}
	/* 发送失败，客户端可能已断开 */
	connDestroy(conn);
}

void connClose(connection *conn, char *data, int raw) {
	if (conn->status == CONN_CONNECTING) {
		connDestroy(conn);
		return;
	}

	if (conn->status == CONN_CLOSING || conn->status == CONN_CLOSED) {
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
	if (conn->status == CONN_CLOSED) {
		return;
	}

	/* 删除可读、可写事件 */
	aeDeleteFileEvent(server.el, conn->fd, (AE_READABLE|AE_WRITABLE));

    close(conn->fd);
    free(conn);
}
