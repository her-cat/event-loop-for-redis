#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>
#include <getopt.h>
#include "../../src/ae/ae.h"
#include "connection.h"
#include "server.h"
#include "config.h"

struct httpServer server;

static const struct option commandOptions[] = {
    {"help", no_argument, NULL, '?'},
    {"version", no_argument, NULL, 'v'},
    {"conf", required_argument, NULL, 'c'},
};

static void usage() {
    fprintf(stderr,
            "simple-http-server [option]... \n"
            "  -c|--conf <config file>  Specify config file.\n"
            "  -?|-h|--help             Display help information.\n"
            "  -v|--version             Display server version.\n"
    );
}

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

	char buf[strlen(conn->sendBuffer)+40];
    memset(buf, 0, strlen(buf));
	sprintf(buf, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n%s", (int)strlen(conn->sendBuffer), conn->sendBuffer);

	connSend(conn, buf, CONN_SEND_RAW);
    connDestroy(conn);
}

void parseRequest(connection *conn, char *buffer) {
	/* TODO: 解析 http 协议头 */
    strcpy(conn->sendBuffer, buffer);
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

void initServerConfig(void) {
	server.pid = getpid();
	server.fd = 0;
	server.port = 8080;
	server.tcp_backlog = 128;
	server.configfile = NULL;
	server.document_root = NULL;
	server.el = NULL;
}

void parseCommand(int argc, char *argv[]) {
    int opt, index = 0;

    while ((opt = getopt_long(argc, argv, "vc:?h", commandOptions, &index)) >= 0) {
        switch (opt) {
            case 'v':
                printf("Server Version: %.1f\n", SERVER_VERSION);
                exit(0);
            case 'c':
                server.configfile = strdup(optarg);
                break;
            case '?':
            case 'h':
            default:
                usage();
                exit(0);
        }
    }
}

void initServer(void) {
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    if (server.configfile != NULL) {
        loadServerConfig(server.configfile);
    }

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


int main(int argc, char *argv[]) {
    initServerConfig();

    parseCommand(argc, argv);

	initServer();

	aeCreateFileEvent(server.el, server.fd, AE_READABLE, acceptTcpHandler, NULL);

	aeMain(server.el);

	return 0;
}
