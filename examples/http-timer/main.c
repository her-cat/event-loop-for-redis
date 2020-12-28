#include <stdio.h>
#include <unistd.h>
#include <bits/types/struct_timeval.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../../src/ae/ae.h"
#include "until.h"

#define URL_MAX_SIZE 1024
#define URL_DEFAULT_PORT 80
#define URL_DEFAULT_INTERVAL_TIME 10 * 1000
#define URL_DEFAULT_RUNNING_TIME 10 * 1000

#define HTTP_DEFAULT_HEADER "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n"

enum URL_STATUS {
    READY,
    RUNNING,
};

typedef enum URL_PARSE_STATUS {
    SCHEME,
    HOST,
    PORT,
    PATH
} URL_PARSE_STATUS;

typedef struct url_s {
    int port;
    int is_ssl;
    char *host;
    char *path;
    char **ip_list;
    enum URL_STATUS status;
    struct timeval last_run_time;
} url_t;

static aeEventLoop *el;

int url_parse(url_t *url, char *buf) {
    char *pos = buf, *host_begin = NULL, *host_end = NULL,
        *port_begin = NULL, *port_end = NULL, *path_begin = NULL;
    enum URL_PARSE_STATUS status = SCHEME;

    while (*pos) {
        switch (status) {
            case SCHEME:
                if (strncasecmp("http://", pos, 7) == 0) {
                    url->is_ssl = 0;
                    status = HOST;
                    pos += 7;
                    continue;
                } else if (strncasecmp("https://", pos, 8) == 0) {
                    url->is_ssl = 1;
                    status = HOST;
                    pos += 8;
                    continue;
                } else {
                    return -1;
                }
            case HOST:
                if (host_begin == NULL)
                    host_begin = pos;

                if (pos[0] == ':') {
                    status = PORT;
                    host_end = pos;
                    break;
                }

                if (pos[0] == '/' || pos[0] == '?') {
                    status = PATH;
                    host_end = pos;
                    continue;
                }
                break;
            case PORT:
                if (port_begin == NULL)
                    port_begin = pos;
                if (pos[0] < '0' || pos[0] > '9') {
                    status = PATH;
                    port_end = pos;
                    continue;
                }
                break;
            case PATH:
                if (path_begin == NULL)
                    path_begin = pos;
                break;
            default:
                return -1;
        }
        pos++;
    }

    if (host_end == NULL)
        host_end = pos;
    else if (port_begin != NULL && port_end == NULL)
        port_end = pos;

    url->host = calloc(1, host_end - host_begin);
    memcpy(url->host, host_begin, host_end - host_begin);

    if (path_begin != NULL && path_begin != pos) {
        url->path = calloc(1, pos - path_begin);
        memcpy(url->path, path_begin, pos - path_begin);
    }

    url->port = url->is_ssl ? 443 : 80;
    if (port_begin != NULL && port_end != NULL) {
        if (str2int(port_begin, port_end - port_begin, &url->port) < 0)
            return -1;
    }

    return 1;
}

url_t *url_init(char *buf) {
    url_t *url;
    struct hostent *ent;

    url = malloc(sizeof(url_t));
    if (url == NULL)
        goto err;

    if (url_parse(url, buf) < 0)
        goto err;

    ent = gethostbyname(url->host);
    if (ent == NULL)
        goto err;

    url->status = READY;
    url->ip_list = ent->h_addr_list;

    return url;

err:
    if (url) {
        free(url->host);
        free(url->path);
        free(url);
    }
    return NULL;
}

static int make_socket_client(url_t *url) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
        return -1;

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(url->port);
    server_addr.sin_addr = *(struct in_addr *)url->ip_list[0];

    socklen_t server_len = sizeof(server_addr);
    int connect_rt = connect(socket_fd, (struct sockaddr *) &server_addr,server_len);
    if (connect_rt < 0)
        return -1;
    return socket_fd;
}

void fileProc(aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
    char buf[4096];
    url_t *url = clientData;

    read(fd, buf, 4096);

    printf("received: %s, file proc executed.\n", buf);

    url->status = READY;
    aeDeleteFileEvent(eventLoop, fd, AE_READABLE | AE_WRITABLE);
}

int timeProc(aeEventLoop *eventLoop, long long id, void *clientData) {
    int socket_fd, ret;
    url_t *url = clientData;
    char header[strlen(url->path) + strlen(url->host) + 60];

    if (url->status == RUNNING) {
        printf("timer_id: %lld, running.\n", id);
        return URL_DEFAULT_RUNNING_TIME;
    }

    url->status = RUNNING;

    socket_fd = make_socket_client(url);
    if (socket_fd < 0) {
        perror("make socket fd failed");
        return AE_NOMORE;
    }

    /* TODO: use host */
    sprintf(header, HTTP_DEFAULT_HEADER, url->path, url->host);

    puts(header);

    if (write(socket_fd, header, strlen(header)) < 0) {
        perror("send http request failed");
        goto err;
    }

    ret = aeCreateFileEvent(eventLoop, socket_fd, AE_READABLE, fileProc, (void *) url);
    if (ret == AE_ERR) {
        perror("create file event failed");
        goto err;
    }

    gettimeofday(&url->last_run_time, NULL);

    printf("id: %lld, now: %ld %d, time proc executed.\n", id, url->last_run_time.tv_sec, (int)url->last_run_time.tv_usec/1000);

    return 10 * 1000;

    err:
    url->status = READY;
    return URL_DEFAULT_INTERVAL_TIME;
}

int main(void) {
    FILE *fp;
    char buf[URL_MAX_SIZE];

    el = aeCreateEventLoop(URL_MAX_SIZE);

    fp = fopen("./urls.txt", "r");
    if (fp == NULL) {
        perror("can't open url.txt");
        return 0;
    }

    while (fgets(buf, URL_MAX_SIZE, fp)) {
        trim(buf);
        if (strlen(buf) == 0)
            continue;

        url_t *url = url_init(buf);
        if (url == NULL) {
            printf("init url: %s failed", buf);
            continue;
        }

        aeCreateTimeEvent(el, 1 * 1000, timeProc, (void *) url, NULL);
    }

    aeMain(el);

    return 0;
}
