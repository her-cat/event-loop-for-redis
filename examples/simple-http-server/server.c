#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "../../src/ae/ae.h"
#include "connection.h"
#include "server.h"

struct httpServer server;

int createWebServer(int port, int backlog) {
	int fd, on = 1;
	struct sockaddr_in server_addr;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("create server socket failed");
		return SERVER_ERR;
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (bind(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		perror("bind server socket failed");
		return SERVER_ERR;
	}

	if (listen(fd, backlog) < 0) {
		perror("listen server socket failed");
		return SERVER_ERR;
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

void parseRequest(connection *conn, char *buffer) {
	/* TODO: 解析 http 协议头 */
	printf("parseRequest[%d]:\n%s\n", conn->fd, buffer);

	aeCreateFileEvent(server.el, conn->fd, AE_WRITABLE, sendResponseToClient, conn);
}

void acceptTcpHandler(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
	int cfd, ret;
	struct sockaddr_storage sa;
	socklen_t salen = sizeof(sa);
	connection *conn;

	cfd = accept(fd, (struct sockaddr *) &sa, &salen);
	if (cfd <= 0) {
		perror("accept failed");
		return;
	}

	if ((conn = connCreate(cfd)) == NULL) {
		perror("create connection failed");
		return;
	}

	conn->onMessage = parseRequest;
	conn->clientAddr = inet_ntoa((((struct sockaddr_in *)&sa))->sin_addr);
	conn->clientPort = ntohs((((struct sockaddr_in *)&sa))->sin_port);

	ret = aeCreateFileEvent(eventLoop, cfd, AE_READABLE, connRead, conn);
	if (ret == AE_ERR) {
		perror("create read file event failed");
	}
}

void initServer(void) {
	server.pid = getpid();
	server.port = 8080;
	server.tcp_backlog = 128;
	server.el = aeCreateEventLoop(1024);
	if (server.el == NULL) {
		perror("create the event loop failed");
		exit(1);
	}

	server.fd = createWebServer(server.port, server.tcp_backlog);
	if (server.fd == SERVER_ERR) {
		exit(1);
	}
}


int main() {
	initServer();

	aeCreateFileEvent(server.el, server.fd, AE_READABLE, acceptTcpHandler, NULL);

	aeMain(server.el);

	return 0;
}
