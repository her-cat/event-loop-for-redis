#ifndef __SERVER_H__
#define __SERVER_H__

#include <fcntl.h>
#include "../../src/ae/ae.h"

#define SERVER_ERR (-1)
#define SERVER_VERSION 0.1

struct httpServer {
	pid_t pid; /* server 进程 ID。 */
	int fd; /* server 监听的 fd。 */
	int port; /* server 监听的端口。 */
	int tcp_backlog; /* tcp backlog 大小。 */
	char *configfile; /* 配置文件路径。 */
	char *document_root; /* 静态文件所在目录。 */
	aeEventLoop *el; /* 事件循环指针。 */
};

extern struct httpServer server;

#endif
