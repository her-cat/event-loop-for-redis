#ifndef __SERVER_H__
#define __SERVER_H__

#include <fcntl.h>
#include "../../src/ae/ae.h"

#define SERVER_ERR -1

struct httpServer {
	pid_t pid;
	int fd;
	int port;
	int tcp_backlog;
	aeEventLoop *el;
};

extern struct httpServer server;

#endif
