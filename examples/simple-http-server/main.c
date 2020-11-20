#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "../../src/ae/ae.h"
#include "connection.h"

#define ERR -1
#define BACKLOG 128

int createWebServer(int port, int backlog) {
    int fd, on = 1;
    struct sockaddr_in server_addr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("create server socket failed.");
        return ERR;
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    fcntl(fd, F_SETFL, O_NONBLOCK);

    if (bind(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printf("bind server socket failed.");
        return ERR;
    }

    if (listen(fd, backlog) < 0) {
        printf("listen server socket failed.");
        return ERR;
    }

    return fd;
}

void sendResponseToClient(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    connection *conn = (connection *)clientData;

    aeDeleteFileEvent(eventLoop, fd, (AE_READABLE|AE_WRITABLE));
    char buf[conn->recvBytes+40];

    memset(buf, 0, strlen(buf));
    sprintf(buf, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n%s", conn->recvBytes, conn->recvBuffer);
    int n = write(fd, buf, strlen(buf));
    close(fd);
}

void parseRequest(aeEventLoop *eventLoop, int fd, connection *conn) {

    /* TODO: 解析 http 协议头 */
    printf("parseRequest[%d]:\n%s\n", fd, conn->recvBuffer);

    aeCreateFileEvent(eventLoop, fd, AE_WRITABLE, sendResponseToClient, conn);
}

void acceptTcpHandler(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    int cfd;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    connection *conn;

    cfd = accept(fd, (struct sockaddr *) &sa, &salen);

    conn = connCreate(cfd);

    conn->onMessage = parseRequest;

    aeCreateFileEvent(eventLoop, cfd, AE_READABLE, connRead, conn);
}

int main() {
    int listen_fd;
    aeEventLoop *eventLoop;

    eventLoop = aeCreateEventLoop(1024);
    assert(eventLoop == NULL);

    listen_fd = createWebServer(8080, BACKLOG);
    assert(listen_fd == ERR);

    aeCreateFileEvent(eventLoop, listen_fd, AE_READABLE, acceptTcpHandler, NULL);

    aeMain(eventLoop);

    return 0;
}
