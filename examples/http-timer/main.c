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
    URL_STATUS_READY,
    URL_STATUS_RUNNING,
};

typedef struct url_s {
    char url[URL_MAX_SIZE];
    enum URL_STATUS status;
    struct timeval last_run_time;
} url_t;

static aeEventLoop *el;

static int make_socket_client(url_t *url) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
        return -1;

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(URL_DEFAULT_PORT);
    /* TODO: use host */
    inet_pton(AF_INET, url->url, &server_addr.sin_addr);

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

    url->status = URL_STATUS_READY;
    aeDeleteFileEvent(eventLoop, fd, AE_READABLE | AE_WRITABLE);
}

int timeProc(aeEventLoop *eventLoop, long long id, void *clientData) {
    int socket_fd, ret;
    url_t *url = clientData;
    char header[strlen(url->url) + 60];

    if (url->status == URL_STATUS_RUNNING) {
        printf("url: %s is running.\n", url->url);
        return URL_DEFAULT_RUNNING_TIME;
    }

    url->status = URL_STATUS_RUNNING;

    socket_fd = make_socket_client(url);
    if (socket_fd < 0) {
        perror("make socket fd failed");
        return AE_NOMORE;
    }

    /* TODO: use host */
    sprintf(header, HTTP_DEFAULT_HEADER, "/", url->url);

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
    url->status = URL_STATUS_READY;
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

        url_t *url = malloc(sizeof(url_t));
        /* TODO: check url */
        strncpy(url->url, buf, strlen(buf));
        url->status = URL_STATUS_READY;

        aeCreateTimeEvent(el, 1 * 1000, timeProc, (void *) url, NULL);
    }

    aeMain(el);

    return 0;
}
