

#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "arpa/inet.h"

#define MAX_LINE 4096

int main(int argc, char **argv) {
    int socket_fd;
    struct sockaddr_in servaddr;

    if (argc != 2) {
        perror("usage: client <IPaddress>");
        return EXIT_FAILURE;
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    int connect_rt = connect(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (connect_rt < 0) {
        perror("connect failed");
    }

    int n, can_read = 0;
    char recv_line[MAX_LINE];

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    fd_set read_mask;
    fd_set all_reads;

    FD_ZERO(&all_reads);
    FD_SET(socket_fd, &all_reads);
    FD_SET(STDIN_FILENO, &all_reads);

    for (;;) {
        read_mask = all_reads;
        int rc = select(socket_fd + 1, &read_mask, NULL, NULL, &tv);
        if (rc < 0) {
            perror("select failed");
        }

        if (FD_ISSET(STDIN_FILENO, &read_mask)) {
            char buf[4096];
            can_read = 1;
            fgets(buf, 4096, stdin);
            write(socket_fd, buf, strlen(buf));
            printf("stdin can read\n");
        }

        if (FD_ISSET(socket_fd, &read_mask)) {
            printf("socket fd can read\n");
            if (can_read) {
                n = read(socket_fd, recv_line, MAX_LINE);
                if (n < 0) {
                    perror("read error");
                } else if (n == 0) {
                    perror("server terminated \n");
                }
            }
        }
        sleep(1);
    }

    exit(0);
}